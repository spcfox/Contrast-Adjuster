#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

float K_R = 0.299f, K_G = 0.587f, K_B = 0.114f;

struct Color
{
	unsigned short x;
	unsigned short y;
	unsigned short z;
};

struct AnalogColor
{
	float x;
	float y;
	float z;
};

struct Image
{
	unsigned int w;
	unsigned int h;
	unsigned int max;
	char bytesPerChannel;
	struct Color* px;
};

struct AnalogImage
{
	unsigned int w;
	unsigned int h;
	struct AnalogColor* px;
};

struct Image* createImage(unsigned int w, unsigned int h, unsigned int max) {
	if (w <= 0 || h <= 0 || max <= 0 || max > 65535) {
		printf("Incorrect format: width and hieght should be positive, maxval shoud be between 1 and 65535\n");
	}

	struct Image* img = malloc(sizeof(struct Image));
	img->w = w;
	img->h = h;
	img->max = max;
	img->bytesPerChannel = img->max > 255 ? 2 : 1;
	img->px = malloc(w * h * sizeof(struct Color));
	return img;
}

struct AnalogImage* createAnalogImage(unsigned int w, unsigned int h) {
	struct AnalogImage* img = malloc(sizeof(struct AnalogImage));
	img->w = w;
	img->h = h;
	img->px = malloc(w * h * sizeof(struct AnalogColor));
	return img;
}

void removeImage(struct Image* img) {
	free(img->px);
	free(img);
}

void removeAnalogImage(struct AnalogImage* aImg) {
	free(aImg->px);
	free(aImg);
}

char isWhitespace(unsigned char ch) {
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

char isDigit(unsigned char ch) {
	return ch >= '0' && ch <= '9';
}

char skipWhitespaces(unsigned char* buffer, unsigned long* pos) {
	if (!isWhitespace(buffer[*pos])) {
		return 0;
	}
	while (isWhitespace(buffer[++(*pos)]));
	return 1;
}

unsigned int readInt(unsigned char* buffer, unsigned long* pos) {
	if (!isDigit(buffer[*pos])) {
		printf("Incorrect format: unexpected symbol `%c`, expecteed digit\n", buffer[*pos]);
		exit(1);
	}
	unsigned int res = 0;
	while(isDigit(buffer[*pos])) {
		res *= 10;
		res += (unsigned int) (buffer[(*pos)++] - '0');
	}
	return res;
}

unsigned short readPxChannel(unsigned char* buffer, unsigned long pos, int bytesPerChannel) {
	unsigned short res = 0;
	for (int i = 0; i < bytesPerChannel; i++) {
		res <<= 8u;
		res += buffer[pos];
	}
	return res;
}

struct Color readPx(unsigned char* buffer, unsigned long pos, int bytesPerChannel) {
	struct Color color = {
		readPxChannel(buffer, pos++, bytesPerChannel),
		readPxChannel(buffer, pos++, bytesPerChannel),
		readPxChannel(buffer, pos, bytesPerChannel)
	};
	return color;
}

char* readFile(char* filename, unsigned long* size) {
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("File not found\n");
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	*size = (unsigned long) ftell(fp);
	fseek(fp, 0, SEEK_SET);

	unsigned char* buffer = malloc(*size * sizeof(char));
	fread(buffer, sizeof(char), *size, fp);
	fclose(fp);

	return buffer;
}

struct Image* parsePpmHeader(char* buffer, unsigned long size, unsigned long* pos) {
	if (buffer[0] != 'P' || buffer[1] != '6') {
		printf("This program only for P6 images\n");
		exit(1);
	}

	*pos = 2;
	if (!skipWhitespaces(buffer, pos)) {
		printf("Incorrect format: no whitespaces found after magic numbers\n");
		exit(1);
	}
	unsigned int w = readInt(buffer, pos);

	if (!skipWhitespaces(buffer, pos)) {
		printf("Incorrect format: no spaces found after width\n");
		exit(1);
	}
	unsigned int h = readInt(buffer, pos);

	if (!skipWhitespaces(buffer, pos)) {
		printf("Incorrect format: no spaces found after height\n");
		exit(1);
	}
	unsigned int max = readInt(buffer, pos);

	if (!isWhitespace(buffer[(*pos)++])) {
		printf("Incorrect format: no space found after maxval\n");
		exit(1);
	}
	
	struct Image* img = createImage(w, h, max);

	if (size - *pos != 3L * img->bytesPerChannel * img->w * img->h) {
		printf("Incorrect format: incorrect size of file\n");
		exit(1);
	}

	return img;
}

void parsePpmImage(char* buffer, struct Image* img, unsigned long pos) {
	for (unsigned int i = 0; i < img->h; i++) {
		int pxNum = i * img->w;
		for (unsigned int j = 0; j < img->w; j++) {
			img->px[pxNum] = readPx(buffer, pos + 3 * pxNum, img->bytesPerChannel);
			pxNum++;
		}
	}
}

struct Image* parsePpm(char* buffer, unsigned long size) {
	unsigned long pos;
	struct Image* img = parsePpmHeader(buffer, size, &pos);
	parsePpmImage(buffer, img, pos);
	return img;
}

void parsePpmImageOmp(char* buffer, struct Image* img, unsigned long pos) {
	#pragma omp parallel for schedule(runtime)
	for (unsigned int i = 0; i < img->h; i++) {
		int pxNum = i * img->w;
		for (unsigned int j = 0; j < img->w; j++) {
			img->px[pxNum] = readPx(buffer, pos + 3 * pxNum, img->bytesPerChannel);
			pxNum++;
		}
	}
}

struct Image* parsePpmOmp(char* buffer, unsigned long size) {
	unsigned long pos;
	struct Image* img = parsePpmHeader(buffer, size, &pos);
	parsePpmImageOmp(buffer, img, pos);
	return img;
}

struct Image* getRedChannel(struct Image* src) {
	struct Image* img = createImage(src->w, src->h, src->max);
	unsigned long pixels = (unsigned long) img->w * (unsigned long) img->h;
	img->px = malloc(pixels * sizeof(struct Color));
	for (unsigned int i = 0; i < img->h; i++) {
		for (unsigned int j = 0; j < img->w; j++) {
			unsigned long pxNum = i * img->w + j;
			struct Color* px = &(img->px[pxNum]);
			px->x = src->px[pxNum].x;
			px->y = src->px[pxNum].x;
			px->z = src->px[pxNum].x;
		}
	}
	return img;
}

void printPxChannel(FILE* fp, unsigned short x, char bytesPerChannel) {\
	unsigned char a, b;
	switch (bytesPerChannel) {
		case 1:
			fputc(x, fp);
			break;
		case 2:
			b = (unsigned char) (x & (1 << 8));
			a = (unsigned char) (x >> 8);
			fputc(a, fp);
			fputc(b, fp);
			break;
		default:
			printf("Error: incorrect bytes per channel: %d\n", bytesPerChannel);
			exit(2);
	}
}

void printPx(FILE* fp, struct Image* img, long pxNum) {
	struct Color px = img->px[pxNum];
	printPxChannel(fp, px.x, img->bytesPerChannel);
	printPxChannel(fp, px.y, img->bytesPerChannel);
	printPxChannel(fp, px.z, img->bytesPerChannel);
}

void savePpm(char* filename, struct Image* img) {
	FILE* fp = fopen(filename, "wb");
	fputs("P6\n", fp);
	fprintf(fp, "%d %d\n%d\n", img->w, img->h, img->max);
	for (unsigned int i = 0; i < img->h; i++) {
		for (unsigned int j = 0; j < img->w; j++) {
			printPx(fp, img, i * img->w + j);
		}
	}
	fclose(fp);
}

void fixBound(float* x) {
	if (*x < 0) {
		*x = 0.0f;
	} else if (*x > 1) {
		*x = 1.0f;
	}
}

struct AnalogColor colorToAnalog(struct Color color, unsigned short maxValue) {
	struct AnalogColor aColor = {
		(float) color.x / maxValue, 
		(float) color.y / maxValue, 
		(float) color.z / maxValue
	};
	return aColor;
}

struct Color colorToInt(struct AnalogColor aColor, unsigned maxValue) {
	struct Color color = {
		(unsigned short) (aColor.x * maxValue), 
		(unsigned short) (aColor.y * maxValue), 
		(unsigned short) (aColor.z * maxValue)
	};
	return color;
}

struct AnalogColor rgbToYCbCr(struct AnalogColor aRgb) {
	struct AnalogColor aYCbCr = {};
	aYCbCr.x = K_R * aRgb.x + K_G * aRgb.y + K_B * aRgb.z;
	aYCbCr.y = (aRgb.z - aYCbCr.x) / (1 - K_B);
	aYCbCr.z = (aRgb.x - aYCbCr.x) / (1 - K_R);
	return aYCbCr;
}

struct AnalogColor yCbCrToRgb(struct AnalogColor aYCbCr) {
	struct AnalogColor aRgb = {};
	aRgb.x = aYCbCr.x + aYCbCr.z * (1 - K_R);
	aRgb.z = aYCbCr.x + aYCbCr.y * (1 - K_B);
	aRgb.y = (aYCbCr.x - K_R * aRgb.x - K_B * aRgb.z) / K_G;
	fixBound(&aRgb.x);
	fixBound(&aRgb.y);
	fixBound(&aRgb.z);
	return aRgb;
}

struct Image* contrast(struct Image* src) {
	float darkest = 1.0;
	float brightest = 0.0;
	struct AnalogImage* aImg = createAnalogImage(src->w, src->h);

	for (unsigned int i = 0; i < src->h; i++) {
		for (unsigned int j = 0; j < src->w; j++) {
			unsigned long pxNum = i * src->w + j;
			struct AnalogColor* px = &(aImg->px[pxNum]);

			*px = rgbToYCbCr(colorToAnalog((src->px[pxNum]), src->max));

			if (px->x < darkest) {
				darkest = px->x;
			}
			if (px->x > brightest) {
				brightest = px->x;
			}
		}
	}

	if (darkest == brightest) {
		return src;
	}

	struct Image* img = createImage(src->w, src->h, src->max);

	for (unsigned int i = 0; i < aImg->h; i++) {
		for (unsigned int j = 0; j < aImg->w; j++) {
			unsigned long pxNum = i * aImg->w + j;

			struct AnalogColor* px = &(aImg->px[pxNum]);
			px->x = (px->x - darkest) / (brightest - darkest);
			img->px[pxNum] = colorToInt(yCbCrToRgb((aImg->px[pxNum])), img->max);
		}
	}
	removeAnalogImage(aImg);

	return img;
}

struct Image* contrastOmp(struct Image* src) {
	float darkest = 1.0;
	float brightest = 0.0;
	struct AnalogImage* aImg = createAnalogImage(src->w, src->h);

	#pragma omp parallel for schedule(runtime) reduction(min:darkest) reduction(max:brightest)
	for (unsigned int i = 0; i < src->h; i++) {
		for (unsigned int j = 0; j < src->w; j++) {
			unsigned long pxNum = i * src->w + j;
			struct AnalogColor* px = &(aImg->px[pxNum]);

			*px = rgbToYCbCr(colorToAnalog((src->px[pxNum]), src->max));

			if (px->x < darkest) {
				darkest = px->x;
			}
			if (px->x > brightest) {
				brightest = px->x;
			}
		}
	}

	if (darkest == brightest) {
		return src;
	}

	struct Image* img = createImage(src->w, src->h, src->max);

	#pragma omp parallel for schedule(runtime)
	for (unsigned int i = 0; i < aImg->h; i++) {
		for (unsigned int j = 0; j < aImg->w; j++) {
			unsigned long pxNum = i * aImg->w + j;

			struct AnalogColor* px = &(aImg->px[pxNum]);
			px->x = (px->x - darkest) / (brightest - darkest);
			img->px[pxNum] = colorToInt(yCbCrToRgb((aImg->px[pxNum])), img->max);
		}
	}
	removeAnalogImage(aImg);

	return img;
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Please, run program with 3 arguments:\n<threads> <input file> <output file>\n");
		exit(1);
	}

	int threads = atoi(argv[1]);

	if (threads > 0) {
		omp_set_num_threads(threads);
	} else if (threads < 0) {
		printf("Threads cannot be negative\n");
		exit(1);
	}

	omp_set_schedule(omp_sched_guided, 1);
	
	unsigned long size;
	char* input = readFile(argv[2], &size);

	struct Image* img;
	struct Image* contrastImg;

	clock_t t1 = clock();
	img = parsePpm(input, size);
	contrastImg = contrast(img);
	t1 = clock() - t1;
	free(img);
	free(contrastImg);

	clock_t t2 = clock();
	img = parsePpmOmp(input, size);
	contrastImg = contrastOmp(img);
	t2 = clock() - t2;

	free(input);
	savePpm(argv[3], contrastImg);
	free(img);
	free(contrastImg);

	printf("Time: %d ms\n", t1);
	printf("Time (%i thread(s)): %d ms\n", omp_get_max_threads(), t2);

	return 0;
}
