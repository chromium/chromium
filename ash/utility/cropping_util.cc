// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/cropping_util.h"

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace ash {

SkBitmap CenterCropImage(const SkBitmap& image, const gfx::Size& target_size) {
  DCHECK(!image.empty());
  DCHECK(!image.isNull());
  DCHECK(!target_size.IsEmpty());
  const int orig_width = image.width();
  const int orig_height = image.height();
  const int new_width = target_size.width();
  const int new_height = target_size.height();

  // The dimension with the smallest ratio must be cropped, the other
  // one is preserved. Both are set in gfx::Size cropped_size.
  double horizontal_ratio =
      static_cast<double>(new_width) / static_cast<double>(orig_width);
  double vertical_ratio =
      static_cast<double>(new_height) / static_cast<double>(orig_height);
  gfx::Size cropped_size;
  if (vertical_ratio > horizontal_ratio) {
    cropped_size =
        gfx::Size(base::ClampRound(new_width / vertical_ratio), orig_height);
    DCHECK_LE(cropped_size.width(), orig_width);
  } else {
    cropped_size =
        gfx::Size(orig_width, base::ClampRound(new_height / horizontal_ratio));
    DCHECK_LE(cropped_size.height(), orig_height);
  }
  gfx::Rect cropped_rect(orig_width, orig_height);
  cropped_rect.ClampToCenteredSize(cropped_size);
  SkBitmap sub_image;
  if (!image.extractSubset(&sub_image, gfx::RectToSkIRect(cropped_rect))) {
    NOTREACHED() << "Cropping image with dimensions "
                 << gfx::Size(orig_width, orig_height).ToString() << " to "
                 << cropped_rect.ToString() << " failed.";
  }
  return sub_image;
}

}  // namespace ash
