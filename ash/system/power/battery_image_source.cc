// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_image_source.h"

#include <algorithm>
#include <cmath>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/power/power_status.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// The minimum height (in dp) of the charged region of the battery icon when the
// battery is present and has a charge greater than 0.
const int kMinVisualChargeLevel = 1;

// These dimensions specify the largest possible rectangle that is fully
// encompassed by the battery icon |kBatteryIcon|. This rectangle is the area
// that is "filled" to show battery charge percentage.
constexpr gfx::RectF kDefaultFillRect = gfx::RectF(7, 6, 6, 10);

}  // namespace

namespace ash {

BatteryImageSource::BatteryImageSource(
    const PowerStatus::BatteryImageInfo& info,
    int height,
    const BatteryColors& resolved_colors)
    : gfx::CanvasImageSource(gfx::Size(height, height)),
      info_(info),
      resolved_colors_(resolved_colors) {}

BatteryImageSource::~BatteryImageSource() = default;

void BatteryImageSource::Draw(gfx::Canvas* canvas) {
  // Draw the solid outline of the battery icon.
  PaintVectorIcon(canvas, kBatteryIcon, size().height(),
                  resolved_colors_.foreground_color);

  canvas->Save();

  const float dsf = canvas->UndoDeviceScaleFactor();

  // All constants below are expressed relative to a canvas size of 20. The
  // actual canvas size (i.e. |size()|) may not be 20.
  const float kAssumedCanvasSize = 20;
  const float const_scale = dsf * size().height() / kAssumedCanvasSize;

  SkPath path;

  gfx::RectF fill_rect = kDefaultFillRect;
  fill_rect.Scale(const_scale);
  path.addRect(gfx::RectToSkRect(gfx::ToEnclosingRect(fill_rect)));
  cc::PaintFlags flags;

  SkRect icon_bounds = path.getBounds();

  // |charge_level| is a value between 0 and the visual height of the icon
  // representing the number of device pixels the battery image should be
  // shown charged. The exception is when |charge_level| is very low; in this
  // case, still draw 1 dip of charge. There are only 10 dips to indicate charge
  // level. If the level is always floor rounded (as was the historical
  // behavior) 25% charge looks very low. Similarly, always rounding normally
  // makes 75% look abnormally high. To help mitigate this, UX prefers floor
  // rounding above 50%, and normal rounding below 50%.
  const float unrounded_charge_level =
      info_.charge_percent / 100.0 * icon_bounds.height();
  float charge_level = info_.charge_percent <= 50
                           ? std::round(unrounded_charge_level)
                           : std::floor(unrounded_charge_level);

  const float min_charge_level = dsf * kMinVisualChargeLevel;
  charge_level =
      std::clamp(charge_level, min_charge_level, icon_bounds.height());

  const float charge_y = icon_bounds.bottom() - charge_level;
  gfx::RectF clip_rect(0, charge_y, size().width() * dsf,
                       size().height() * dsf);
  canvas->ClipRect(clip_rect);

  const bool use_alert_color =
      charge_level == min_charge_level && info_.alert_if_low;
  flags.setColor(use_alert_color ? resolved_colors_.alert_color
                                 : resolved_colors_.foreground_color);
  canvas->DrawPath(path, flags);

  canvas->Restore();

  if (info_.badge_outline) {
    // The outline is always a vector icon with PATH_MODE_CLEAR. This means it
    // masks out anything previously drawn to the canvas. Give it any opaque
    // color so it will properly mask the rest of the battery icon. NOTE: The
    // view which renders this canvas must paint to its own non-opaque layer,
    // otherwise this outline will show up SK_ColorBlue instead of
    // transparent.
    PaintVectorIcon(canvas, *info_.badge_outline, size().height(),
                    SK_ColorBLUE);
  }

  // Paint the badge over top of the battery, if applicable.
  if (info_.icon_badge) {
    SkColor badge_color = use_alert_color ? resolved_colors_.alert_color
                                          : resolved_colors_.badge_color;
    PaintVectorIcon(canvas, *info_.icon_badge, size().height(), badge_color);
  }
}

bool BatteryImageSource::HasRepresentationAtAllScales() const {
  return true;
}

}  // namespace ash
