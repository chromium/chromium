// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_view.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

AmbientAnimationView::AmbientAnimationView(
    const AmbientBackendModel* model,
    AmbientViewEventHandler* event_handler,
    std::unique_ptr<const AmbientAnimationStaticResources> static_resources)
    : event_handler_(event_handler),
      static_resources_(std::move(static_resources)),
      animation_photo_provider_(static_resources_.get(), model) {
  SetID(AmbientViewID::kAmbientAnimationView);
  Init();
}

AmbientAnimationView::~AmbientAnimationView() = default;

void AmbientAnimationView::Init() {
  SetUseDefaultFillLayout(true);
  animated_image_view_ =
      AddChildView(std::make_unique<views::AnimatedImageView>());
  base::span<const uint8_t> lottie_data_bytes =
      base::as_bytes(base::make_span(static_resources_->GetLottieData()));
  // Create a serializable SkottieWrapper since the SkottieWrapper may have to
  // be serialized and transmitted over IPC for out-of-process rasterization.
  auto animation = std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
          lottie_data_bytes.begin(), lottie_data_bytes.end())),
      cc::SkottieColorMap(), &animation_photo_provider_);
  animation->SetAnimationObserver(this);
  animated_image_view_->SetAnimatedImage(std::move(animation));
}

void AmbientAnimationView::AnimationWillStartPlaying(
    const lottie::Animation* animation) {
  event_handler_->OnMarkerHit(AmbientPhotoConfig::Marker::kUiStartRendering);
}

void AmbientAnimationView::AnimationCycleEnded(
    const lottie::Animation* animation) {
  event_handler_->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
}

void AmbientAnimationView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  DVLOG(4) << __func__ << " from " << previous_bounds.ToString() << " to "
           << bounds().ToString();
  if (size().IsEmpty())
    return;

  // By default, the |animated_image_view_| will render the animation with the
  // fixed dimensions specified in the Lottie file. To render the animation
  // at the view's full bounds, wait for the view's initial layout to happen
  // so that its proper bounds become available (they are 0x0 initially) before
  // starting the animation playback.
  DVLOG(4) << "View bounds available. Beginning rendering...";
  animated_image_view_->SetImageSize(size());
  animated_image_view_->Play();
}

BEGIN_METADATA(AmbientAnimationView, views::View)
END_METADATA

}  // namespace ash
