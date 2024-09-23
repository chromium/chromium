// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {checkTransparency} from './transparency.js';

export const SUPPORTED_FILE_TYPES = [
  'image/bmp',
  'image/heic',
  'image/heif',
  'image/jpeg',
  'image/png',
  'image/tiff',
  'image/webp',
  'image/x-icon',
];

type MimeType = typeof SUPPORTED_FILE_TYPES[number];

const MIME_TYPE_TO_EXTENSION_MAP: ReadonlyMap<MimeType, string> =
    new Map<MimeType, string>([
      ['image/png', '.png'],
      ['image/webp', '.webp'],
      ['image/bmp', '.bmp'],
      ['image/heif', '.heif'],
      ['image/jpeg', '.jpg'],
      ['image/tiff', '.tif'],
      ['image/heic', '.heic'],
      ['image/x-icon', '.ico'],
    ]);

const MAX_LONGEST_EDGE_PIXELS = 1000;

const TRANSPARENCY_FILL_BG_COLOR = '#ffffff';

const JPEG_QUALITY = 0.4;

const DEFAULT_MIME_TYPE = 'image/jpeg' as MimeType;

export interface ProcessedFile {
  processedFile: File;
  imageWidth?: number;
  imageHeight?: number;
}

// Takes an image file and does the following:
//   1. Downscales the image so that the longest edge is maxLongestEdgePixels
//      (default 1000 px). Aspect ratio is preserved.
//   2. Transcodes the image to jpeg, filling in the background with white if
//      the original image had transparency.
// The processed image is returned along with the new image dimensions.
export async function processFile(
    file: File, maxLongestEdgePixels: number = MAX_LONGEST_EDGE_PIXELS):
    Promise<ProcessedFile> {
  const image = await readImageFile(file);
  if (!image) {
    return {processedFile: file};
  }

  const originalImageWidth = image.width;
  const originalImageHeight = image.height;

  const hasTransparency = checkTransparency(await file.arrayBuffer());

  const blobInfo = await processImage(
      image, DEFAULT_MIME_TYPE, hasTransparency, maxLongestEdgePixels);

  if (!blobInfo || !blobInfo.blob) {
    return {
      processedFile: file,
      imageWidth: originalImageWidth,
      imageHeight: originalImageHeight,
    };
  }

  const processedImage = blobInfo.blob;
  let imageWidth = blobInfo.imageWidth;
  let imageHeight = blobInfo.imageHeight;

  const lastDot = file.name.lastIndexOf('.');

  const fileName = `${lastDot > 0 ? file.name.slice(0, lastDot) : file.name}${
      MIME_TYPE_TO_EXTENSION_MAP.get(processedImage.type as MimeType)}`;

  let processedFile = new File(
      [processedImage], fileName,
      {lastModified: Date.now(), type: processedImage.type});

  if (processedFile.size > file.size) {
    processedFile = file;
    imageWidth = originalImageWidth;
    imageHeight = originalImageHeight;
  }
  return {processedFile, imageWidth, imageHeight};
}

async function readImageFile(file: File): Promise<HTMLImageElement|null> {
  const dataUrl = await readAsDataURL(file);

  if (!dataUrl || dataUrl instanceof ArrayBuffer) {
    return null;
  }

  return createImageFromDataUrl(dataUrl);
}

function processImage(
    image: HTMLImageElement, mimeType: MimeType, hasTransparency: boolean,
    maxLongestEdgePixels?: number) {
  const [width, height] = getDimensions(image, maxLongestEdgePixels);

  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;

  const context = canvas.getContext('2d', {alpha: false, desynchronized: true});

  if (!context) {
    return null;
  }

  if (hasTransparency) {
    fillBackground(
        context, canvas.width, canvas.height, TRANSPARENCY_FILL_BG_COLOR);
  }

  context.drawImage(image, /*dx=*/ 0, /*dy=*/ 0, width, height);

  return toBlob(canvas, mimeType, JPEG_QUALITY, width, height);
}

function getDimensions(image: HTMLImageElement, maxLongestEdgePixels?: number):
    [width: number, height: number] {
  let width = image.width;
  let height = image.height;

  if (maxLongestEdgePixels &&
      (width > maxLongestEdgePixels || height > maxLongestEdgePixels)) {
    const downscaleRatio =
        Math.min(maxLongestEdgePixels / width, maxLongestEdgePixels / height);
    width *= downscaleRatio;
    height *= downscaleRatio;
  }

  return [Math.floor(width), Math.floor(height)];
}

function fillBackground(
    context: CanvasRenderingContext2D, canvasWidth: number,
    canvasHeight: number, backgroundColor: string) {
  context.fillStyle = backgroundColor;
  context.fillRect(0, 0, canvasWidth, canvasHeight);
}

function toBlob(
    canvas: HTMLCanvasElement, type: MimeType, encodingCompressionRatio: number,
    imageWidth: number, imageHeight: number) {
  return new Promise<
      {blob: Blob | null, imageWidth: number, imageHeight: number}>(
      (resolve) => {
        canvas.toBlob((result) => {
          if (result) {
            resolve({blob: result, imageWidth, imageHeight});
          } else {
            resolve({blob: null, imageWidth, imageHeight});
          }
        }, type, encodingCompressionRatio);
      });
}

function readAsDataURL(file: File) {
  const fileReader = new FileReader();

  const promise = new Promise<string|ArrayBuffer|null>((resolve) => {
    fileReader.onloadend = () => {
      resolve(fileReader.result);
    };
    fileReader.onerror = () => {
      // Failed to read file.
      resolve(null);
    };
  });

  fileReader.readAsDataURL(file);
  return promise;
}

function createImageFromDataUrl(dataUrl: string) {
  const image = new Image();

  const promise = new Promise<HTMLImageElement|null>((resolve) => {
    image.onload = () => {
      resolve(image);
    };
    image.onerror = () => {
      // Failed to load image from data url.
      resolve(null);
    };
  });

  image.src = dataUrl;
  return promise;
}
