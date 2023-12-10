// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dot_indicator.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

namespace {

// The shadow value installed on the dot indicator.
const gfx::ShadowValues kIndicatorShadow =
    gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(2);

}  // namespace

DotIndicator::DotIndicator(SkColor indicator_color)
    : shadow_values_(kIndicatorShadow), indicator_color_(indicator_color) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetVisible(false);
}

DotIndicator::~DotIndicator() = default;

void DotIndicator::SetColor(SkColor new_color) {
  indicator_color_ = new_color;
  SchedulePaint();
}

void DotIndicator::SetIndicatorBounds(gfx::Rect indicator_bounds) {
  // Include the shadow margin to the bounds.
  indicator_bounds.Inset(gfx::ShadowValue::GetMargin(shadow_values_));
  SetBoundsRect(indicator_bounds);
}

void DotIndicator::OnPaint(gfx::Canvas* canvas) {
  // Return early if the indicator bounds are not set yet.
  if (bounds().IsEmpty())
    return;

  gfx::ScopedCanvas scoped(canvas);
  canvas->SaveLayerAlpha(SK_AlphaOPAQUE);

  const float dsf = canvas->UndoDeviceScaleFactor();

  // Remove the shadow margin to get the indicator bounds without shadow.
  gfx::Rect bounds_without_shadow = bounds();
  gfx::Insets shadow_insets = -gfx::ShadowValue::GetMargin(shadow_values_);
  bounds_without_shadow.Inset(shadow_insets);
  float radius = bounds_without_shadow.width() / 2.0f;

  // Set the center of the dot with the shadow offset.
  gfx::PointF center =
      gfx::PointF(radius + shadow_insets.left(), radius + shadow_insets.top());
  center.Scale(dsf);

  // Fill the center.
  cc::PaintFlags flags;
  flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values_));
  flags.setColor(indicator_color_);
  flags.setAntiAlias(true);
  canvas->DrawCircle(center, dsf * radius, flags);
}

BEGIN_METADATA(DotIndicator)
END_METADATA

}  // namespace ash
