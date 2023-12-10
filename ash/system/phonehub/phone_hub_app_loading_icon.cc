// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_app_loading_icon.h"

#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/animation/animation_builder.h"

namespace ash {

namespace {

// Constants for loading animation
constexpr float kAnimationOpacityHigh = 1.0f;
constexpr float kAnimationOpacityLow = 0.5f;
constexpr int kAnimationFadeDownDurationInMs = 500;
constexpr int kAnimationFadeUpDurationInMs = 500;

class LoadingCircle : public gfx::CanvasImageSource {
 public:
  explicit LoadingCircle(int size)
      : CanvasImageSource(AppIcon::GetRecommendedImageSize(size)) {}

  LoadingCircle(const LoadingCircle&) = delete;
  LoadingCircle& operator=(const LoadingCircle&) = delete;

  void Draw(gfx::Canvas* canvas) override {
    float radius = size().width() / 2.0f;
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
    canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
  }
};

}  // namespace

AppLoadingIcon::AppLoadingIcon(int size)
    : AppIcon(gfx::Image(
                  gfx::CanvasImageSource::MakeImageSkia<LoadingCircle>(size)),
              size) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
}

AppLoadingIcon::~AppLoadingIcon() = default;

void AppLoadingIcon::StartLoadingAnimation(
    std::optional<base::TimeDelta> initial_delay) {
  if (initial_delay) {
    animation_initial_delay_timer_.Start(
        FROM_HERE, *initial_delay,
        base::BindOnce(&AppLoadingIcon::StartLoadingAnimation,
                       base::Unretained(this),
                       /*initial_delay=*/std::nullopt));
    return;
  }

  views::AnimationBuilder builder;
  animation_abort_handle_ = builder.GetAbortHandle();
  builder.Repeatedly()
      .SetDuration(base::Milliseconds(kAnimationFadeDownDurationInMs))
      .SetOpacity(this, kAnimationOpacityLow, gfx::Tween::ACCEL_30_DECEL_20_85)
      .Then()
      .SetDuration(base::Milliseconds(kAnimationFadeUpDurationInMs))
      .SetOpacity(this, kAnimationOpacityHigh, gfx::Tween::LINEAR);
}

void AppLoadingIcon::StopLoadingAnimation() {
  animation_abort_handle_.reset();
  animation_initial_delay_timer_.Stop();
}

BEGIN_METADATA(AppLoadingIcon)
END_METADATA

}  // namespace ash
