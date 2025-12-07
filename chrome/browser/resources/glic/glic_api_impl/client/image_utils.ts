// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {RgbaImage} from '../request_types.js';

class BitmapHeaderGenerator {
  template: ArrayBuffer;
  readonly bytesPerPixel = 4;
  readonly fileHeaderSize = 14;

  // Offsets of each field. 14 bytes total.
  // https://en.wikipedia.org/wiki/BMP_file_format#Bitmap_file_header
  readonly bitmapFileHeaderFields = {
    'bfType': 0,     // 2 bytes
    'bFileSize': 2,  // 4 bytes
    // 'bReserved1': 6, // 2 bytes
    // 'bReserved2': 8, // 2 bytes
    'bOffBits': 10,  // 4 bytes
  } as const;

  // Offsets of each header field, 124 bytes total.
  // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv5header
  readonly bitmapv5HeaderFields = {
    'bV5Size': 0,          // 4 bytes
    'bV5Width': 4,         // 4 bytes
    'bV5Height': 8,        // 4 bytes
    'bV5Planes': 12,       // 2 bytes
    'bV5BitCount': 14,     // 2 bytes
    'bV5Compression': 16,  // 4 bytes
    // 'bV5SizeImage': 20, // 4 bytes
    // 'bV5XPelsPerMeter': 24, // 4 bytes
    // 'bV5YPelsPerMeter': 28, // 4 bytes
    // 'bV5ClrUsed': 32, // 4 bytes
    // 'bV5ClrImportant': 36, // 4 bytes
    // 'bV5RedMask': 40, // 4 bytes
    // 'bV5GreenMask': 44, // 4 bytes
    // 'bV5BlueMask': 48, // 4 bytes
    'bV5AlphaMask': 52,  // 4 bytes
    'bV5CSType': 56,     // 4 bytes
    // 'bV5Endpoints': 60, // 36 bytes
    // 'bV5GammaRed': 96, // 4 bytes
    // 'bV5GammaGreen': 100, // 4 bytes
    // 'bV5GammaBlue': 104, // 4 bytes
    'bV5Intent': 108,  // 4 bytes
    // 'bV5ProfileData': 112, // 4 bytes
    // 'bV5ProfileSize': 116, // 4 bytes
    // 'bV5Reserved': 120, // 4 bytes
  } as const;

  readonly totalHeaderSize = 138;  // 14 + 124

  constructor() {
    this.template = this.createTemplate();
  }

  createBmpHeader(width: number, height: number): ArrayBuffer {
    // Copy the template.
    const buffer = this.template.slice(0);
    const dataView = new DataView(buffer);
    const pixelSize = Math.abs(width * height * this.bytesPerPixel);
    {
      const F = this.bitmapFileHeaderFields;
      dataView.setUint32(F.bFileSize, this.totalHeaderSize + pixelSize, true);
    }
    {
      const F = this.bitmapv5HeaderFields;
      const OFFSET = this.fileHeaderSize;
      dataView.setInt32(OFFSET + F.bV5Width, width, true);
      dataView.setInt32(OFFSET + F.bV5Height, height, true);
    }

    return buffer;
  }

  // Creates a template for the bitmap header, which sets fields that
  // will not change depending on the image.
  private createTemplate(): ArrayBuffer {
    // Values here should match those in
    // skia/ext/skia_utils_win.cc.

    // Create the buffer for the header
    const buffer = new ArrayBuffer(this.totalHeaderSize);

    // Use a DataView to write bytes into the buffer.
    // In setUint*(), true = use little-endian
    const dataView = new DataView(buffer);
    {
      const F = this.bitmapFileHeaderFields;
      dataView.setUint8(F.bfType, 0x42);      // 'B'
      dataView.setUint8(F.bfType + 1, 0x4D);  // 'M'
      dataView.setUint32(F.bOffBits, this.totalHeaderSize, true);
    }
    {
      const F = this.bitmapv5HeaderFields;
      const OFFSET = this.fileHeaderSize;
      dataView.setUint32(OFFSET + F.bV5Size, 124, true);
      dataView.setUint16(OFFSET + F.bV5Planes, 1, true);
      dataView.setUint16(OFFSET + F.bV5BitCount, 32, true);
      dataView.setUint32(OFFSET + F.bV5Compression, 0 /*BI_RGB*/, true);
      dataView.setUint32(OFFSET + F.bV5AlphaMask, 0xff000000, true);
      dataView.setUint32(
          OFFSET + F.bV5CSType, 0x57696E20 /*LCS_WINDOWS_COLOR_SPACE*/, true);
      dataView.setUint32(OFFSET + F.bV5Intent, 4 /*LCS_GM_IMAGES*/, true);
    }
    return buffer;
  }
}

const bitmapHeaderGenerator = new BitmapHeaderGenerator();

// Converts an RgbaImage into a Blob. Output is a BMP.
export async function rgbaImageToBmpBlob(image: RgbaImage): Promise<Blob> {
  const result = Promise.resolve(new Blob(
      [
        // Note: A negative height indicates the pixel data's first row is at
        // the top of the image instead of the bottom.
        bitmapHeaderGenerator.createBmpHeader(image.width, -image.height),
        image.dataRGBA,
      ],
      {
        type: 'image/bmp',
      }));

  return result;
}
