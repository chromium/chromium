// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_image_source.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/cxx17_backports.h"
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

inline SkColor GetBatteryBadgeColor() {
  return ash::AshColorProvider::Get()->GetContentLayerColor(
      ash::AshColorProvider::ContentLayerType::kBatteryBadgeColor);
}

inline SkColor GetAlertColor() {
  return ash::AshColorProvider::Get()->GetContentLayerColor(
      ash::AshColorProvider::ContentLayerType::kIconColorAlert);
}

}  // namespace

namespace ash {

BatteryImageSource::BatteryImageSource(
    const PowerStatus::BatteryImageInfo& info,
    int height,
    SkColor bg_color,
    SkColor fg_color,
    absl::optional<SkColor> badge_color)
    : gfx::CanvasImageSource(gfx::Size(height, height)),
      info_(info),
      bg_color_(bg_color),
      fg_color_(fg_color),
      badge_color_(badge_color.value_or(
          info.charge_percent > 50 ? GetBatteryBadgeColor() : fg_color)) {}

BatteryImageSource::~BatteryImageSource() = default;

void BatteryImageSource::Draw(gfx::Canvas* canvas) {
  // Draw the solid outline of the battery icon.
  PaintVectorIcon(canvas, kBatteryIcon, size().height(), fg_color_);

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
  // case, still draw 1dip of charge.
  float charge_level =
      std::floor(info_.charge_percent / 100.0 * icon_bounds.height());
  const float min_charge_level = dsf * kMinVisualChargeLevel;
  charge_level =
      base::clamp(charge_level, min_charge_level, icon_bounds.height());

  const float charge_y = icon_bounds.bottom() - charge_level;
  gfx::RectF clip_rect(0, charge_y, size().width() * dsf,
                       size().height() * dsf);
  canvas->ClipRect(clip_rect);

  const SkColor alert_color = GetAlertColor();
  const bool use_alert_color =
      charge_level == min_charge_level && info_.alert_if_low;
  flags.setColor(use_alert_color ? alert_color : fg_color_);
  canvas->DrawPath(path, flags);

  canvas->Restore();

  if (info_.badge_outline) {
    if (ash::features::IsDarkLightModeEnabled()) {
      // The outline is always a vector icon with PATH_MODE_CLEAR. This means it
      // masks out anything previously drawn to the canvas. Give it any opaque
      // color so it will properly mask the rest of the battery icon. NOTE: The
      // view which renders this canvas must paint to its own non-opaque layer,
      // otherwise this outline will show up SK_ColorBlue instead of
      // transparent.
      PaintVectorIcon(canvas, *info_.badge_outline, size().height(),
                      SK_ColorBLUE);
    } else {
      // The outline is a colored outline, so give it a color meant to be seen
      // by the user.
      const SkColor outline_color =
          info_.charge_percent > 50 ? fg_color_ : bg_color_;
      PaintVectorIcon(canvas, *info_.badge_outline, size().height(),
                      outline_color);
    }
  }

  // Paint the badge over top of the battery, if applicable.
  if (info_.icon_badge) {
    const SkColor default_color =
        ash::features::IsDarkLightModeEnabled() ? fg_color_ : badge_color_;
    const SkColor badge_color = use_alert_color ? alert_color : default_color;
    PaintVectorIcon(canvas, *info_.icon_badge, size().height(), badge_color);
  }
}

bool BatteryImageSource::HasRepresentationAtAllScales() const {
  return true;
}

}  // namespace ash
