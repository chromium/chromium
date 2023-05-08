// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/booting/booting_animation_view.h"

#include <string>

#include "ash/public/cpp/image_util.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/lottie/animation.h"
#include "ui/views/background.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

void Resize(views::AnimatedImageView& animated_image_view) {
  DCHECK(animated_image_view.animated_image());
  gfx::Size animation_size =
      animated_image_view.animated_image()->GetOriginalSize();
  DCHECK(!animation_size.IsEmpty());
  gfx::Rect destination_bounds = animated_image_view.GetContentsBounds();
  DCHECK(!destination_bounds.IsEmpty());
  gfx::Size animation_resized;
  const float width_scale_factor =
      static_cast<float>(destination_bounds.width()) / animation_size.width();
  const float height_scale_factor =
      static_cast<float>(destination_bounds.height()) / animation_size.height();

  const bool scale_to_width = width_scale_factor > height_scale_factor;
  if (scale_to_width) {
    animation_resized.set_width(destination_bounds.width());
    animation_resized.set_height(
        base::ClampRound(animation_size.height() * width_scale_factor));
  } else {
    animation_resized.set_height(destination_bounds.height());
    animation_resized.set_width(
        base::ClampRound(animation_size.width() * height_scale_factor));
  }
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

}  // namespace

BootingAnimationView::BootingAnimationView(const std::string& animation_data) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto skottie = cc::SkottieWrapper::CreateSerializable(
      std::vector<uint8_t>(animation_data.begin(), animation_data.end()));
  AddChildView(
      views::Builder<views::AnimatedImageView>()
          .CopyAddressTo(&animation_)
          .SetAnimatedImage(std::make_unique<lottie::Animation>(skottie))
          .Build());
  animation_->SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  animated_image_view_observer_.Observe(animation_);
}

BootingAnimationView::~BootingAnimationView() = default;

void BootingAnimationView::Play() {
  animation_->Play(lottie::Animation::PlaybackConfig::CreateWithStyle(
      lottie::Animation::Style::kLinear, *animation_->animated_image()));
}

void BootingAnimationView::OnViewBoundsChanged(View* observed_view) {
  gfx::Rect content_bounds = observed_view->GetContentsBounds();
  if (content_bounds.IsEmpty()) {
    return;
  }

  // By default, the |animated_image_view_| will render the animation with the
  // fixed dimensions specified in the Lottie file. To render the animation
  // at the view's full bounds, wait for the view's initial layout to happen
  // so that its proper bounds become available (they are 0x0 initially) before
  // starting the animation playback.
  gfx::Rect previous_animation_bounds = animation_->GetImageBounds();
  Resize(*animation_);
  VLOG(1) << "View bounds available. Resized animation with native size "
          << animation_->animated_image()->GetOriginalSize().ToString()
          << " from " << previous_animation_bounds.ToString() << " to "
          << animation_->GetImageBounds().ToString();
}

}  // namespace ash
