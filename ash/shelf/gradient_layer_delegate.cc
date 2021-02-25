// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/gradient_layer_delegate.h"

#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/paint_info.h"

namespace ash {

GradientLayerDelegate::GradientLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
  layer_.set_delegate(this);
  layer_.SetFillsBoundsOpaquely(false);
}

GradientLayerDelegate::~GradientLayerDelegate() {
  layer_.set_delegate(nullptr);
}

void GradientLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  const gfx::Size size = layer()->size();

  views::PaintInfo paint_info =
      views::PaintInfo::CreateRootPaintInfo(context, size);
  const auto& paint_recording_size = paint_info.paint_recording_size();

  // Pass the scale factor when constructing PaintRecorder so the MaskLayer
  // size is not incorrectly rounded (see https://crbug.com/921274).
  ui::PaintRecorder recorder(
      context, paint_info.paint_recording_size(),
      static_cast<float>(paint_recording_size.width()) / size.width(),
      static_cast<float>(paint_recording_size.height()) / size.height(),
      nullptr);

  recorder.canvas()->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);

  if (!start_fade_zone_.zone_rect.IsEmpty())
    DrawFadeZone(start_fade_zone_, recorder.canvas());
  if (!end_fade_zone_.zone_rect.IsEmpty())
    DrawFadeZone(end_fade_zone_, recorder.canvas());
}

void GradientLayerDelegate::DrawFadeZone(const FadeZone& fade_zone,
                                         gfx::Canvas* canvas) {
  gfx::Point start_point;
  gfx::Point end_point;
  if (fade_zone.is_horizontal) {
    start_point = gfx::Point(fade_zone.zone_rect.x(), 0);
    end_point = gfx::Point(fade_zone.zone_rect.right(), 0);
  } else {
    start_point = gfx::Point(0, fade_zone.zone_rect.y());
    end_point = gfx::Point(0, fade_zone.zone_rect.bottom());
  }

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setAntiAlias(false);

  flags.setShader(gfx::CreateGradientShader(
      start_point, end_point,
      fade_zone.fade_in ? SK_ColorTRANSPARENT : SK_ColorBLACK,
      fade_zone.fade_in ? SK_ColorBLACK : SK_ColorTRANSPARENT));

  canvas->DrawRect(fade_zone.zone_rect, flags);
}

}  // namespace ash
