// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/point_scan_layer.h"

#include "ash/shell.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const int kDefaultStrokeWidth = 6;
constexpr int kDefaultRangeWidthDips = 150;
constexpr int kDefaultRangeHeightDips = 120;
constexpr int kDefaultPaddingDips = 4;

display::Display GetPrimaryDisplay() {
  DCHECK(display::Screen::GetScreen());
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}
}  // namespace

PointScanLayer::PointScanLayer(AccessibilityLayerDelegate* delegate,
                               PointScanLayer::Orientation orientation,
                               PointScanLayer::Type type)
    : AccessibilityLayer(delegate), orientation_(orientation), type_(type) {
  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(GetPrimaryDisplay().id());
  CreateOrUpdateLayer(root_window, "PointScanning", gfx::Rect(),
                      /*stack_at_top=*/true);
  SetOpacity(1.0);
  layer()->SetBounds(GetPrimaryDisplay().bounds());
}

void PointScanLayer::Start() {
  gfx::Point start;
  gfx::Point end;

  // Set the end point, based on the orientation.
  if (orientation_ == PointScanLayer::Orientation::HORIZONTAL)
    end = bounds().bottom_left();
  else if (orientation_ == PointScanLayer::Orientation::VERTICAL)
    end = bounds().top_right();

  // Ranges need to offset |line_| by the range width.
  if (type_ == PointScanLayer::Type::RANGE) {
    if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
      start.Offset(kDefaultPaddingDips, 0);
      end.Offset(kDefaultPaddingDips, 0);
    } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
      start.Offset(0, kDefaultPaddingDips);
      end.Offset(0, kDefaultPaddingDips);
    }
  }

  line_.start = start;
  line_.end = end;
  is_moving_ = true;
}

void PointScanLayer::Pause() {
  is_moving_ = false;
}

bool PointScanLayer::IsMoving() const {
  return is_moving_;
}

bool PointScanLayer::CanAnimate() const {
  return true;
}
bool PointScanLayer::NeedToAnimate() const {
  return true;
}
int PointScanLayer::GetInset() const {
  return 0;
}

void PointScanLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kDefaultStrokeWidth);
  flags.setColor(gfx::kGoogleBlue300);

  SkPath path;

  if (type_ == PointScanLayer::Type::RANGE) {
    if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
      path.moveTo(line_.start.x() + kDefaultRangeWidthDips, line_.start.y());
      path.lineTo(line_.end.x() + kDefaultRangeWidthDips, line_.end.y());
    } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
      path.moveTo(line_.start.x(), line_.start.y() + kDefaultRangeHeightDips);
      path.lineTo(line_.end.x(), line_.end.y() + kDefaultRangeHeightDips);
    }
  }

  path.moveTo(line_.start.x(), line_.start.y());
  path.lineTo(line_.end.x(), line_.end.y());
  recorder.canvas()->DrawPath(path, flags);
}

}  // namespace ash
