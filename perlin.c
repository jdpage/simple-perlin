#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <jpeglib.h>

#define GET_GRAD(x, y) grid[x + (y * pitch)]

typedef struct {
	double x;
	double y;
} pair;

typedef struct {
	unsigned char *buf;
	int channels;
	int height;
	int width;
	int pitch;
	J_COLOR_SPACE space;
} image;

image *new_image(int width, int height, int channels);
void del_image(image *i);
pair mkpair(double x, double y);
void add(image *i, image *j);

int write_jpeg(const char *file, image *img);

int perlin2d(image *i, int xsectors, int ysectors, int seed, double amp);

int main(int argc, char **argv) {
	image *i, *j;
	int xsize = 0, ysize = 0, counter = 1, k;
	double amp = 1.0;
	char *p;
	
	if (argc != 4) {
		fprintf(stderr, "syntax: perlin xsize ysize outfile\n");
		return 1;
	}
	
	for (p = argv[1]; *p != 0; p++)
		xsize = xsize * 10 + (*p - '0');
	for (p = argv[2]; *p != 0; p++)
		ysize = ysize * 10 + (*p - '0');
	
	if ((i = new_image(512, 512, 3)) == NULL)
		goto mem_error;
	if ((j = new_image(512, 512, 3)) == NULL)
		goto mem_error;

	for (k = 0; k < 3; k++) {
		if (perlin2d(i, xsize, ysize, time(NULL) + k, 1) == 1)
			goto mem_error;
		i->buf++;
	}
	i->buf -= 3;
	
	amp /= 2;
	xsize << 1;
	ysize << 1;
	
	while (xsize < 512 && ysize < 512 && amp > 1/256.0) {
		for (k = 0; k < 3; k++) {
			if (perlin2d(j, xsize, ysize, time(NULL) + k, amp) == 1)
				goto mem_error;
			j->buf++;
		}
		j->buf -= 3;
		
		add(i, j);
		
		amp /= 2;
		xsize *= 2;
		ysize *= 2;
		counter++;
	}
	
	printf("depth: %d\n", counter);
	
	if (write_jpeg(argv[3], i) == 1) {
		fprintf(stderr, "could not open file %s\n", argv[3]);
		return 1;
	}
	
	del_image(i);
	del_image(j);

	return 0;
	
	mem_error:
	
	fprintf(stderr, "memory error :(\n");
	return 1;
}

/* Writes JPEGs */

int write_jpeg(const char *file, image *img) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	int row_stride;
	FILE *output;
	
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	
	if ((output = fopen(file, "wb")) == NULL) {
		fprintf(stderr, "error opening file %s", file);
		return 1;
	}
	
	jpeg_stdio_dest(&cinfo, output); 
	cinfo.image_width = img->width;
	cinfo.image_height = img->height;
	cinfo.input_components = img->channels;
	cinfo.in_color_space = img->space;
	jpeg_set_defaults(&cinfo);

	jpeg_start_compress(&cinfo, TRUE);

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = & img->buf[cinfo.next_scanline * img->pitch];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	
	fclose(output);
}

/* Constructor and destructor for image struct */

image *new_image(int width, int height, int channels) {
	image *i;
	
	if ((i = (image *)malloc(sizeof(image))) == NULL)
		return NULL;
	
	i->width = width;
	i->height = height;
	i->channels = channels;
	if (channels == 1) i->space = JCS_GRAYSCALE;
	else if (channels == 3) i->space = JCS_RGB;
	i->pitch = width * channels;
	
	if ((i->buf = (unsigned char *)malloc(width * height * channels * sizeof(char))) == NULL) {
		free(i);
		return NULL;
	}
	
	return i;
}

void del_image(image *i) {
	free(i->buf);
	free(i);
}

pair mkpair(double x, double y) { pair k; k.x = x; k.y = y; return k; }

void add(image *i, image *j) {
	int k, v;
	
	/* foolishly assume same size */
	for (k = 0; k < i->width * i->height * i->channels; k++) {
		v = i->buf[k] + (j->buf[k] - 128);
		if (v < 0) v = 0;
		else if (v > 255) v = 255;
		i->buf[k] = (unsigned char)v;
	}
}

/* Perlin 2d */

double dotproduct(pair a, pair b) {
	return a.x * b.x + a.y * b.y;
}

void mkrandgrid(pair *grid, double amp, int size) {
	int k, degs;
	double rads;

	for (k = 0; k < size; k++) {
		degs = rand() % 360;
		rads = degs * M_PI / 180;
		grid[k].x = amp * cos(rads);
		grid[k].y = amp * sin(rads);
	}
}

double ease(double p) { return 3 * pow(p, 2) - 2 * pow(p, 3); }

double perlin2d_noise(pair coord, pair* grid, int pitch) {
	int x0, y0, x1, y1;
	pair a, b, c, d, e, f, g, h;
	double s, t, u, v, Sx, Sy, i, j;

	x0 = (int)(coord.x); y0 = (int)(coord.y);
	x1 = x0 + 1; y1 = y0 + 1;
	
	
	a = GET_GRAD(x0, y0);
	b = GET_GRAD(x1, y0);
	c = GET_GRAD(x0, y1);
	d = GET_GRAD(x1, y1);
	
	e.x = g.x = coord.x - x0; e.y = f.y = coord.y - y0;
	f.x = h.x = coord.x - x1; g.y = h.y = coord.y - y1;

	s = dotproduct(a, e);
	t = dotproduct(b, f);
	u = dotproduct(c, g);
	v = dotproduct(d, h);

	Sx = ease(coord.x - x0);
	Sy = ease(coord.y - y0);

	i = s + Sx * (t - s);
	j = u + Sx * (v - u);

	return i + Sy * (j - i);
}

int perlin2d(image *i, int xsectors, int ysectors, int seed, double amp) {
	pair *grid;
	int k, xsize, ysize;
	pair c;
	
	if (seed == -1)
		seed = time(NULL);
	
	srand(seed);
	
	xsize = i->width / xsectors;
	ysize = i->height / ysectors;
	
	if ((grid = malloc(xsectors * (ysectors + 1) * sizeof(pair))) == NULL)
		return 1;
	
	mkrandgrid(grid, amp, xsectors * (ysectors + 1));
	
	for (k = 0; k < (i->width * i->height); k++) {
		c.x = (k % i->width) / (double)xsize;
		c.y = (k / i->width) / (double)ysize;
		i->buf[k * i->channels] = (unsigned char)(perlin2d_noise(c, grid, xsectors) * 128 + 128);
	}
	
	free(grid);
	
	return 0;
}





