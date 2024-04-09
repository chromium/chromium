// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/booting/booting_animation_view.h"

#include <string>

#include "ash/public/cpp/image_util.h"
#include "base/i18n/rtl.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
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

BootingAnimationView::BootingAnimationView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  animation_ = AddChildView(std::make_unique<views::AnimatedImageView>());
}

BootingAnimationView::~BootingAnimationView() = default;

void BootingAnimationView::Play() {
  animation_->Play(lottie::Animation::PlaybackConfig::CreateWithStyle(
      lottie::Animation::Style::kLinear, *animation_->animated_image()));
}

void BootingAnimationView::SetAnimatedImage(const std::string& animation_data) {
  auto skottie = cc::SkottieWrapper::UnsafeCreateSerializable(
      std::vector<uint8_t>(animation_data.begin(), animation_data.end()));
  if (!skottie->is_valid()) {
    LOG(ERROR) << "Invalid animation data.";
    return;
  }
  animation_->SetAnimatedImage(std::make_unique<lottie::Animation>(skottie));

  // Make animation cover the widget.
  if (base::i18n::IsRTL()) {
    gfx::Rect content_bounds = animation_->GetContentsBounds();
    gfx::Transform transform;
    transform.RotateAboutYAxis(180.0);
    transform.Translate(-content_bounds.width(), 0);
    animation_->SetTransform(transform);
  }
  Resize(*animation_);
}

lottie::Animation* BootingAnimationView::GetAnimatedImage() {
  return animation_ ? animation_->animated_image() : nullptr;
}

}  // namespace ash
