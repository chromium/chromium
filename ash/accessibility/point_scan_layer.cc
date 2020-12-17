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

PointScanLayer::PointScanLayer(AccessibilityLayerDelegate* delegate)
    : AccessibilityLayer(delegate) {
  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(GetPrimaryDisplay().id());
  CreateOrUpdateLayer(root_window, "PointScanning", gfx::Rect(),
                      /*stack_at_top=*/true);
  SetOpacity(1.0);
  bounds_ = GetPrimaryDisplay().bounds();
  layer()->SetBounds(bounds_);
}

void PointScanLayer::StartHorizontalRangeScanning() {
  is_range_scan_ = true;
  gfx::Point start(kDefaultPaddingDips, 0);
  gfx::Point end(bounds_.bottom_left().x() + kDefaultPaddingDips,
                 bounds_.bottom_left().y());
  line_.start = start;
  line_.end = end;
  is_moving_ = true;
  is_horizontal_range_ = true;
}

void PointScanLayer::StartHorizontalScanning() {
  is_range_scan_ = false;
  gfx::Point end = bounds_.bottom_left();
  bounds_.set_origin(line_.start);
  line_.end = end;
  is_moving_ = true;
  is_horizontal_range_ = false;
}

void PointScanLayer::StartVerticalRangeScanning() {
  is_range_scan_ = true;
  gfx::Point start(0, kDefaultPaddingDips);
  gfx::Point end(bounds_.top_right().x(),
                 bounds_.top_right().y() + kDefaultPaddingDips);
  line_.start = start;
  line_.end = end;
  is_moving_ = true;
}

void PointScanLayer::StartVerticalScanning() {
  is_range_scan_ = false;
  gfx::Point end = bounds_.top_right();
  bounds_.set_origin(line_.start);
  line_.end = end;
  is_moving_ = true;
}

void PointScanLayer::PauseHorizontalRangeScanning() {
  is_moving_ = false;
}

void PointScanLayer::PauseHorizontalScanning() {
  is_moving_ = false;
}

void PointScanLayer::PauseVerticalRangeScanning() {
  is_moving_ = false;
}

void PointScanLayer::PauseVerticalScanning() {
  is_moving_ = false;
}

gfx::Rect PointScanLayer::GetBounds() const {
  return bounds_;
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

  if (is_range_scan_) {
    if (is_horizontal_range_) {
      path.moveTo(line_.start.x() + kDefaultRangeWidthDips, line_.start.y());
      path.lineTo(line_.end.x() + kDefaultRangeWidthDips, line_.end.y());
    } else {
      path.moveTo(line_.start.x(), line_.start.y() + kDefaultRangeHeightDips);
      path.lineTo(line_.end.x(), line_.end.y() + kDefaultRangeHeightDips);
    }
  }

  path.moveTo(line_.start.x(), line_.start.y());
  path.lineTo(line_.end.x(), line_.end.y());
  recorder.canvas()->DrawPath(path, flags);
}

}  // namespace ash
