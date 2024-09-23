// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/switch_access/point_scan_layer.h"

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
constexpr int kStrokeWidth = 2;
constexpr int kDefaultRangeWidthDips = 150;
constexpr int kDefaultRangeHeightDips = 120;
constexpr int kDashLengthDips = 6;
constexpr int kGapLengthDips = 3;

constexpr SkColor kInnerColor = gfx::kGoogleBlue300;
constexpr SkColor kOuterColor = gfx::kGoogleBlue600;

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
  gfx::Point start = bounds().origin();
  gfx::Point end;
  int padding = kStrokeWidth / 2 + 1;

  // Set the end point, based on the orientation. Offset by padding so lines
  // draw onscreen.
  if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
    end = bounds().bottom_left();
    start.Offset(padding, 0);
    end.Offset(padding, 0);
  } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
    end = bounds().top_right();
    start.Offset(0, padding);
    end.Offset(0, padding);
  }

  line_.start = start;
  line_.end = end;
  is_moving_ = true;
}

void PointScanLayer::Pause() {
  is_moving_ = false;
  layer()->SchedulePaint(bounds());
}

bool PointScanLayer::IsMoving() const {
  return is_moving_;
}

int PointScanLayer::GetInset() const {
  return 0;
}

void PointScanLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kStrokeWidth);
  int half_stroke_width = kStrokeWidth / 2;

  if (!is_moving_) {
    SkScalar intervals[] = {kDashLengthDips, kGapLengthDips};
    int intervals_length = 2;
    flags.setPathEffect(
        cc::PathEffect::MakeDash(intervals, intervals_length, 0));
  }

  flags.setColor(kOuterColor);
  if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
    DrawLineWithOffsets(recorder.canvas(), flags, -half_stroke_width, 0);
  } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
    DrawLineWithOffsets(recorder.canvas(), flags, 0, -half_stroke_width);
  }

  flags.setColor(kInnerColor);
  if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
    DrawLineWithOffsets(recorder.canvas(), flags, half_stroke_width, 0);
  } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
    DrawLineWithOffsets(recorder.canvas(), flags, 0, half_stroke_width);
  }

  if (type_ != PointScanLayer::Type::RANGE)
    return;

  // Draw the second line for range scanning.
  flags.setColor(kOuterColor);
  if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
    DrawLineWithOffsets(recorder.canvas(), flags,
                        kDefaultRangeWidthDips - half_stroke_width, 0);
  } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
    DrawLineWithOffsets(recorder.canvas(), flags, 0,
                        kDefaultRangeHeightDips - half_stroke_width);
  }

  flags.setColor(kInnerColor);
  if (orientation_ == PointScanLayer::Orientation::HORIZONTAL) {
    DrawLineWithOffsets(recorder.canvas(), flags,
                        kDefaultRangeWidthDips + half_stroke_width, 0);
  } else if (orientation_ == PointScanLayer::Orientation::VERTICAL) {
    DrawLineWithOffsets(recorder.canvas(), flags, 0,
                        kDefaultRangeHeightDips + half_stroke_width);
  }
}

void PointScanLayer::DrawLineWithOffsets(gfx::Canvas* canvas,
                                         cc::PaintFlags flags,
                                         int x_offset,
                                         int y_offset) {
  SkPath path;
  path.moveTo(line_.start.x() + x_offset, line_.start.y() + y_offset);
  path.lineTo(line_.end.x() + x_offset, line_.end.y() + y_offset);
  canvas->DrawPath(path, flags);
}

}  // namespace ash
