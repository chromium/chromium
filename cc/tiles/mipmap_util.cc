// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/mipmap_util.h"

#include <algorithm>
#include <limits>

#include "base/numerics/safe_math.h"

namespace cc {
namespace {
// Calculates the size of |axis_base_size| at the given |mip_level|. Note that
// the calculation here rounds up for consistency with size calculations in the
// JPEG decoder. This allows us to decode images to the mip size directly.
int ScaleAxisToMipLevel(int axis_base_size, int mip_level) {
  DCHECK_GE(mip_level, 0);
  DCHECK_LT(mip_level, 32);

  if (mip_level == 0)
    return axis_base_size;

  // Increment the size by (2^mip_level - 1) so we round on when dividing it
  // below.
  base::CheckedNumeric<int> base_size = axis_base_size;
  base_size += (1u << mip_level) - 1u;
  axis_base_size = base_size.ValueOrDefault(std::numeric_limits<int>::max());
  return std::max(1, axis_base_size >> mip_level);
}

}  // namespace

int MipMapUtil::GetLevelForSize(const gfx::Size& src_size,
                                const gfx::Size& target_size) {
  int src_height = src_size.height();
  int src_width = src_size.width();
  int target_height = target_size.height();
  int target_width = target_size.width();
  DCHECK_GT(target_height, 0);
  DCHECK_GT(target_width, 0);
  DCHECK_GT(src_width, 0);
  DCHECK_GT(src_height, 0);

  int next_mip_height = src_height;
  int next_mip_width = src_width;
  for (int current_mip_level = 0;; current_mip_level++) {
    int mip_height = next_mip_height;
    int mip_width = next_mip_width;

    next_mip_height = ScaleAxisToMipLevel(src_height, current_mip_level + 1);
    next_mip_width = ScaleAxisToMipLevel(src_width, current_mip_level + 1);

    // Check if an axis on the next mip level would be smaller than the target.
    // If so, use the current mip level.
    // This effectively always uses the larger image and always scales down.
    if (next_mip_height < target_height || next_mip_width < target_width) {
      return current_mip_level;
    }

    if (mip_height == 1 && mip_width == 1) {
      // We have reached the final mip level
      return current_mip_level;
    }
  }
}

SkSize MipMapUtil::GetScaleAdjustmentForLevel(const gfx::Size& src_size,
                                              int mip_level) {
  DCHECK_GT(src_size.width(), 0);
  DCHECK_GT(src_size.height(), 0);
  DCHECK_GE(mip_level, 0);

  gfx::Size target_size = GetSizeForLevel(src_size, mip_level);

  return SkSize::Make(
      static_cast<float>(target_size.width()) / src_size.width(),
      static_cast<float>(target_size.height()) / src_size.height());
}

gfx::Size MipMapUtil::GetSizeForLevel(const gfx::Size& src_size,
                                      int mip_level) {
  DCHECK_GT(src_size.width(), 0);
  DCHECK_GT(src_size.height(), 0);
  DCHECK_GE(mip_level, 0);

  return gfx::Size(ScaleAxisToMipLevel(src_size.width(), mip_level),
                   ScaleAxisToMipLevel(src_size.height(), mip_level));
}

SkSize MipMapUtil::GetScaleAdjustmentForSize(const gfx::Size& src_size,
                                             const gfx::Size& target_size) {
  int target_mip_level = GetLevelForSize(src_size, target_size);
  return GetScaleAdjustmentForLevel(src_size, target_mip_level);
}

}  // namespace cc
