// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wallpaper/wallpaper_utils/scored_sample.h"

#include <type_traits>
#include <vector>

#include "base/logging.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/material_color_utilities/src/cpp/quantize/celebi.h"
#include "third_party/material_color_utilities/src/cpp/score/score.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

using material_color_utilities::Argb;
using material_color_utilities::QuantizeCelebi;
using material_color_utilities::QuantizerResult;
using material_color_utilities::RankedSuggestions;

// The number of colors that the algorithm will consider when determining what
// is most prominent.
constexpr uint16_t kMaxColors = 16;

std::vector<Argb> ImageToArgb(const SkBitmap* bitmap) {
  static_assert(std::is_same<Argb, uint32_t>::value,
                "Argb must be a 32-bit integer");
  static_assert(0xAABBCCDD == SkColorSetARGB(0xAA, 0xBB, 0xCC, 0xDD),
                "Assert that SkColor is encoded as ARGB.");

  const SkPixmap& pixmap = bitmap->pixmap();
  int64_t num_pixels = pixmap.dimensions().area();
  if (pixmap.colorType() == kBGRA_8888_SkColorType) {
    // Fast path if the buffer is already in the expected format.
    return std::vector<Argb>(pixmap.addr32(), pixmap.addr32() + num_pixels);
  }

  // TODO(b/266948729): Evaluate if there are faster ways to perform this
  // re-packing of the color SkColor integer.
  std::vector<Argb> converted_pixels;
  converted_pixels.reserve(num_pixels);

  // Iterate over the pixels in pixmap. Use getColor instead of copying the
  // underlying memory since the pixmap color type might not be ARGB.
  for (int32_t y = 0; y < pixmap.height(); y++) {
    for (int32_t x = 0; x < pixmap.width(); x++) {
      SkColor pixel = pixmap.getColor(x, y);
      converted_pixels.push_back(pixel);
    }
  }

  return converted_pixels;
}

}  // namespace

SkColor ComputeWallpaperSeedColor(gfx::ImageSkia image) {
  std::vector<Argb> pixels = ImageToArgb(image.bitmap());
  QuantizerResult result = QuantizeCelebi(pixels, kMaxColors);

  if (result.color_to_count.empty()) {
    LOG(WARNING) << "Wallpaper color extraction failed";
    // In the event of an error, return the seed for the default palette.
    return gfx::kGoogleBlue400;
  }

  // TODO(b/314178502): Remove this re-packing when the type of QuantizerResult
  // is fixed.
  std::map<Argb, uint32_t> color_to_count;
  for (const auto& it : result.color_to_count) {
    // Re-pack the color_to_count map so that we can pass it to
    // `RankedSuggestions`.
    color_to_count.emplace(it.first, static_cast<uint32_t>(it.second));
  }

  std::vector<Argb> best_colors = RankedSuggestions(color_to_count);

  return best_colors.front();
}

}  // namespace ash
