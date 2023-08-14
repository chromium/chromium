// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Utility which gives a best effort guess on whether a supplied image file's
 * bytes represent an image with transparency.
 */
export function checkTransparency(buffer: ArrayBuffer): boolean {
  const view = new DataView(buffer);

  return isTransparentPNG(view) || isTransparentBMP(view) ||
      isTransparentWebP(view);
}

/**
 * Safely gets Uint8 value from DataView.
 *
 * Swallows potential RangeErrors, opting to return null in those cases.
 */
function getUint8FromView(view: DataView, offset: number): number|null {
  try {
    return view.getUint8(offset);
  } catch {
    return null;
  }
}

/**
 * Safely gets Uint16 value from DataView.
 *
 * Swallows potential RangeErrors, opting to return null in those cases.
 */
function getUint16FromView(view: DataView, offset: number): number|null {
  try {
    return view.getUint16(offset);
  } catch {
    return null;
  }
}

/**
 * Safely gets Uint32 value from DataView.
 *
 * Swallows potential RangeErrors, opting to return null in those cases.
 */
function getUint32FromView(view: DataView, offset: number): number|null {
  try {
    return view.getUint32(offset);
  } catch {
    return null;
  }
}

/**
 * Whether a DataView represents a PNG image.
 */
export function isPNG(view: DataView): boolean {
  // 89 50 4E 47 is PNG magic number.
  // Next four bytes should always be 0D 0A 1A 0A.
  return getUint32FromView(view, 0) === 0x89504E47 &&
      getUint32FromView(view, 4) === 0x0D0A1A0A;
}

function isTransparentPNG(view: DataView): boolean {
  if (!isPNG(view)) {
    return false;
  }
  // We know format field exists in the IHDR chunk. The chunk exists at
  // offset 8 +8 bytes (size, name) +8 (depth) & +9 (type) = 25 byte offset.
  const type = getUint8FromView(view, 25);
  return type === 0x04 || type === 0x06;  // grayscale + alpha or RGB + alpha
}

/**
 * Whether a DataView represents a WebP image.
 */
export function isWebP(view: DataView): boolean {
  // 52 49 46 46 || <ignore 4 bytes> || 57 45 42 50 is the WebP magic number.
  // R  I  F  F  || <ignore 4 bytes> || W  E  B  P in ASCII.
  // https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
  return getUint32FromView(view, 0) === 0x52494646 &&
      getUint32FromView(view, 8) === 0x57454250;
}

/**
 * Whether a DataView represents a WebP image.
 * Checks WebP format (VP8 | VP8L | VP8X). VP8 never has an alpha channel. We
 * make an assumption that VP8X and VP8L always have alpha channels. That is
 * not true, but does simplify the logic and for practical purposes, this is
 * good enough of an assumption for us.
 */
function isTransparentWebP(view: DataView): boolean {
  if (!isWebP(view)) {
    return false;
  }
  // Fourteenth byte indicates WebP format. There are three variations of WebP
  // encoding: VP8, VP8L, and VP8X.
  //
  // Offset: 12 13 14 15
  // ASCII:  V  P  8  ? <- ? == " "  || "L"  || "X"
  // Hex:    56 50 38 ? <- ? == 0x20 || 0x4C || 0x58
  const format = getUint8FromView(view, 15);
  // 58 indicates VP8X, 4C indicates VP8L.
  return format === 0x58 || format === 0x4C;
}

/**
 * Whether a DataView represents a BMP image.
 */
export function isBMP(view: DataView): boolean {
  // 42 4D is the BMP magic number.
  return getUint16FromView(view, 0) === 0x424D;
}

function isTransparentBMP(view: DataView): boolean {
  if (!isBMP(view)) {
    return false;
  }
  // Check the value of the bit count field, 2 bytes, 28 byte offset.
  return getUint16FromView(view, 28) === 0x32;
}
