// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phone_hub_small_app_loading_icon.h"

#include "ash/style/ash_color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace ash {
class LoadingCircle : public gfx::CanvasImageSource {
 public:
  explicit LoadingCircle() : CanvasImageSource(gfx::Size(18, 18)) {}

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

SmallAppLoadingIcon::SmallAppLoadingIcon()
    : SmallAppIcon(
          gfx::Image(gfx::CanvasImageSource::MakeImageSkia<LoadingCircle>())) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
}
}  // namespace ash