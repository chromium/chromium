// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/network_icon_image_source.h"

#include "ash/public/cpp/ash_constants.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {
namespace network_icon {

namespace {

// TODO(estade): share this alpha with other things in ash (battery, etc.).
// See https://crbug.com/623987 and https://crbug.com/632827
// For now, this value should match the one used in kTrayIconBackgroundAlpha
// in ash/system/tray/tray_constants.cc
constexpr int kSignalStrengthImageBgAlpha = 0x4D;

SkPath CreateArcPath(gfx::RectF oval, float start_angle, float sweep_angle) {
  SkPath path;
  path.setIsVolatile(true);
  path.setFillType(SkPath::kWinding_FillType);
  path.moveTo(oval.CenterPoint().x(), oval.CenterPoint().y());
  path.arcTo(gfx::RectFToSkRect(oval), start_angle, sweep_angle, false);
  path.close();
  return path;
}

}  // namespace

//------------------------------------------------------------------------------
// NetworkIconImageSource

NetworkIconImageSource::NetworkIconImageSource(const gfx::Size& size,
                                               const gfx::ImageSkia& icon,
                                               const Badges& badges)
    : CanvasImageSource(size), icon_(icon), badges_(badges) {}

NetworkIconImageSource::~NetworkIconImageSource() = default;

void NetworkIconImageSource::Draw(gfx::Canvas* canvas) {
  const int width = size().width();
  const int height = size().height();

  // The base icon is centered in both dimensions.
  const int icon_x = (width - icon_.width()) / 2;
  const int icon_y = (height - icon_.height()) / 2;
  canvas->DrawImageInt(icon_, icon_x, icon_y);

  auto paint_badge = [&canvas](const Badge& badge, int x, int y,
                               int badge_size = 0) {
    gfx::ScopedCanvas scoped(canvas);
    canvas->Translate(gfx::Vector2d(x, y));
    if (badge_size)
      gfx::PaintVectorIcon(canvas, *badge.icon, badge_size, badge.color);
    else
      gfx::PaintVectorIcon(canvas, *badge.icon, badge.color);
  };

  // The center badge is scaled and centered over the icon.
  if (badges_.center.icon)
    paint_badge(badges_.center, icon_x, icon_y, icon_.width());

  if (badges_.top_left.icon)
    paint_badge(badges_.top_left, 0, icon_y);
  if (badges_.bottom_left.icon) {
    paint_badge(
        badges_.bottom_left, 0,
        height - gfx::GetDefaultSizeOfVectorIcon(*badges_.bottom_left.icon));
  }
  if (badges_.bottom_right.icon) {
    const int badge_size =
        gfx::GetDefaultSizeOfVectorIcon(*badges_.bottom_right.icon);
    paint_badge(badges_.bottom_right, width - badge_size, height - badge_size);
  }
}

bool NetworkIconImageSource::HasRepresentationAtAllScales() const {
  return true;
}

//------------------------------------------------------------------------------
// SignalStrengthImageSource

SignalStrengthImageSource::SignalStrengthImageSource(ImageType image_type,
                                                     SkColor color,
                                                     const gfx::Size& size,
                                                     int signal_strength,
                                                     int padding)
    : CanvasImageSource(size),
      image_type_(image_type),
      color_(color),
      signal_strength_(signal_strength),
      padding_(padding) {
  if (image_type_ == NONE)
    image_type_ = ARCS;

  DCHECK_GE(signal_strength, 0);
  DCHECK_LT(signal_strength, kNumNetworkImages);
}

SignalStrengthImageSource::~SignalStrengthImageSource() = default;

// gfx::CanvasImageSource:
void SignalStrengthImageSource::Draw(gfx::Canvas* canvas) {
  if (image_type_ == ARCS)
    DrawArcs(canvas);
  else
    DrawBars(canvas);
}

bool SignalStrengthImageSource::HasRepresentationAtAllScales() const {
  return true;
}

void SignalStrengthImageSource::DrawArcs(gfx::Canvas* canvas) {
  gfx::RectF oval_bounds((gfx::Rect(size())));
  oval_bounds.Inset(gfx::Insets(padding_));
  // Double the width and height. The new midpoint should be the former
  // bottom center.
  oval_bounds.Inset(-oval_bounds.width() / 2, 0, -oval_bounds.width() / 2,
                    -oval_bounds.height());

  constexpr SkScalar kAngleAboveHorizontal = 51.f;
  constexpr SkScalar kStartAngle = 180.f + kAngleAboveHorizontal;
  constexpr SkScalar kSweepAngle = 180.f - 2 * kAngleAboveHorizontal;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  // Background. Skip drawing for full signal.
  if (signal_strength_ != kNumNetworkImages - 1) {
    flags.setColor(SkColorSetA(color_, kSignalStrengthImageBgAlpha));
    canvas->sk_canvas()->drawPath(
        CreateArcPath(oval_bounds, kStartAngle, kSweepAngle), flags);
  }
  // Foreground (signal strength).
  if (signal_strength_ != 0) {
    flags.setColor(color_);
    // Percent of the height of the background wedge that we draw the
    // foreground wedge, indexed by signal strength.
    static constexpr float kWedgeHeightPercentages[] = {0.f, 0.375f, 0.5833f,
                                                        0.75f, 1.f};
    const float wedge_percent = kWedgeHeightPercentages[signal_strength_];
    oval_bounds.Inset(
        gfx::InsetsF((oval_bounds.height() / 2) * (1.f - wedge_percent)));
    canvas->sk_canvas()->drawPath(
        CreateArcPath(oval_bounds, kStartAngle, kSweepAngle), flags);
  }
}

void SignalStrengthImageSource::DrawBars(gfx::Canvas* canvas) {
  // Undo the canvas's device scaling and round values to the nearest whole
  // number so we can draw on exact pixel boundaries.
  const float dsf = canvas->UndoDeviceScaleFactor();
  auto scale = [dsf](SkScalar dimension) {
    return std::round(dimension * dsf);
  };

  // Length of short side of an isosceles right triangle, in dip.
  const SkScalar kFullTriangleSide =
      SkIntToScalar(size().width()) - padding_ * 2;

  auto make_triangle = [scale, kFullTriangleSide, this](SkScalar side) {
    SkPath triangle;
    triangle.moveTo(scale(padding_), scale(padding_ + kFullTriangleSide));
    triangle.rLineTo(scale(side), 0);
    triangle.rLineTo(0, -scale(side));
    triangle.close();
    return triangle;
  };

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  // Background. Skip drawing for full signal.
  if (signal_strength_ != kNumNetworkImages - 1) {
    flags.setColor(SkColorSetA(color_, kSignalStrengthImageBgAlpha));
    canvas->DrawPath(make_triangle(kFullTriangleSide), flags);
  }
  // Foreground (signal strength).
  if (signal_strength_ != 0) {
    flags.setColor(color_);
    // As a percentage of the bg triangle, the length of one of the short
    // sides of the fg triangle, indexed by signal strength.
    static constexpr float kTriangleSidePercents[] = {0.f, 0.375f, 0.5833f,
                                                      0.75f, 1.f};
    canvas->DrawPath(make_triangle(kTriangleSidePercents[signal_strength_] *
                                   kFullTriangleSide),
                     flags);
  }
}

//------------------------------------------------------------------------------

gfx::ImageSkia GetImageForWifiNetwork(SkColor color, gfx::Size size) {
  return gfx::CanvasImageSource::MakeImageSkia<SignalStrengthImageSource>(
      ARCS, color, size, kNumNetworkImages - 1);
}

}  // namespace network_icon
}  // namespace ash
