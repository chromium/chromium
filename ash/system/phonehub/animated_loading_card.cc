// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/system/phonehub/animated_loading_card.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
AnimatedLoadingCard::AnimatedLoadingCard() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
}

AnimatedLoadingCard::~AnimatedLoadingCard() = default;

void AnimatedLoadingCard::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  const SkColor color = SkColorSetRGB(241, 243, 244);

  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeColor(color));
  gfx::Rect local_bounds = gfx::Rect(layer()->size());
  const float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF local_bounds_f = gfx::RectF(local_bounds);
  local_bounds_f.Scale(dsf);
  canvas->DrawRect(gfx::ToEnclosingRect(local_bounds_f), flags);
}
}  // namespace ash
