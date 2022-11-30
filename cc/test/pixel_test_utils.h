// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_UTILS_H_
#define CC_TEST_PIXEL_TEST_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "cc/test/pixel_comparator.h"

class SkBitmap;

namespace cc {

// Encodes a bitmap into a PNG and write to disk. Returns true on success. The
// parent directory does not have to exist.
bool WritePNGFile(const SkBitmap& bitmap, const base::FilePath& file_path,
    bool discard_transparency);

// Reads and decodes a PNG image to a bitmap. Returns true on success. The PNG
// should have been encoded using |gfx::PNGCodec::Encode|.
bool ReadPNGFile(const base::FilePath& file_path, SkBitmap* bitmap);

std::string GetPNGDataUrl(const SkBitmap& bitmap);

// Compares with a PNG file on disk using the given PixelComparator, and returns
// true if the comparator returns a match. |ref_img_path| is absolute.
bool MatchesPNGFile(const SkBitmap& gen_bmp,
                    base::FilePath ref_img_path,
                    const PixelComparator& comparator);

// Compares two bitmaps using the given PixelComparator, and returns true if the
// comparator returns a match.
bool MatchesBitmap(const SkBitmap& gen_bmp,
                   const SkBitmap& ref_bmp,
                   const PixelComparator& comparator);

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_UTILS_H_
