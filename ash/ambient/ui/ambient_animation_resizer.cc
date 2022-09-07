// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_resizer.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {

// static
void AmbientAnimationResizer::Resize(
    views::AnimatedImageView& animated_image_view,
    int padding_for_jitter) {
  DCHECK(animated_image_view.animated_image());
  DCHECK_GE(padding_for_jitter, 0);
  gfx::Size animation_size =
      animated_image_view.animated_image()->GetOriginalSize();
  DCHECK(!animation_size.IsEmpty());
  gfx::Rect destination_bounds = animated_image_view.GetContentsBounds();
  destination_bounds.Outset(padding_for_jitter);
  DCHECK(!destination_bounds.IsEmpty());
  gfx::Size animation_resized;
  float width_scale_factor =
      static_cast<float>(destination_bounds.width()) / animation_size.width();
  animation_resized.set_width(destination_bounds.width());
  // TODO(esum): Add metrics for the number of times the new scaled height
  // is less than the destination height. UX did not intend for this to
  // happen, so it's worth recording.
  animation_resized.set_height(
      base::ClampRound(animation_size.height() * width_scale_factor));
  animated_image_view.SetVerticalAlignment(
      views::ImageViewBase::Alignment::kCenter);
  animated_image_view.SetHorizontalAlignment(
      views::ImageViewBase::Alignment::kCenter);
  // The animation's new scaled size has been computed above.
  // AnimatedImageView::SetImageSize() takes care of both a) applying the
  // scaled size and b) cropping by translating the canvas before painting such
  // that the rescaled animation's origin resides outside the boundaries of the
  // view. The portions of the rescaled animation that reside outside of the
  // view's boundaries ultimately get cropped.
  animated_image_view.SetImageSize(animation_resized);
}

}  // namespace ash
