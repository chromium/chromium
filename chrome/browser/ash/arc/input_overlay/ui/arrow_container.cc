// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/arrow_container.h"

#include "cc/paint/paint_flags.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"

namespace arc::input_overlay {
namespace {

// Whole menu width with arrow.
constexpr int kMenuWidth = 316;

constexpr int kTriangleLength = 20;
constexpr int kTriangleHeight = 14;
constexpr int kCornerRadius = 16;
constexpr int kBorderThickness = 2;
constexpr int kBorderInset = 16;
// Draws the dialog shape path with round corner. It starts after the corner
// radius on line #0 and draws clockwise.
//
// draw_triangle_on_left draws the triangle wedge on the left side of the box
// instead of the right if set to true.
//
// action_offset draws the triangle wedge higher or lower if the position of
// the action is too close to the top or bottom of the window. An offset of
// zero draws the triangle wedge at the vertical center of the box.
//  _0>__________
// |             |
// |             |
// |             |
// |              >
// |             |
// |             |
// |_____________|
//
SkPath BackgroundPath(int height,
                      bool draw_triangle_on_left,
                      int action_offset) {
  SkPath path;
  const int short_length = kMenuWidth - kTriangleHeight - 2 * kCornerRadius;
  const int short_height = height - 2 * kCornerRadius;
  // If the offset is greater than the limit or less than the negative
  // limit, set it respectively.
  const int limit = short_height / 2 - kTriangleLength / 2;
  if (action_offset > limit) {
    action_offset = limit;
  } else if (action_offset < -limit) {
    action_offset = -limit;
  }
  if (draw_triangle_on_left) {
    path.moveTo(kCornerRadius + kTriangleHeight, 0);
  } else {
    path.moveTo(kCornerRadius, 0);
  }
  // Top left after corner radius to top right corner radius.
  path.rLineTo(short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, kCornerRadius, kCornerRadius);
  if (draw_triangle_on_left) {
    // Top right after corner radius to bottom right corner radius.
    path.rLineTo(0, short_height);
  } else {
    // Top right after corner radius to midway point.
    path.rLineTo(0, limit + action_offset);
    // Triangle shape.
    path.rLineTo(kTriangleHeight, kTriangleLength / 2);
    path.rLineTo(-kTriangleHeight, kTriangleLength / 2);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, limit - action_offset);
  }
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, kCornerRadius);
  // Bottom right after corner radius to bottom left corner radius.
  path.rLineTo(-short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, -kCornerRadius);
  if (draw_triangle_on_left) {
    // bottom left after corner radius to midway point.
    path.rLineTo(0, -limit + action_offset);
    // Triangle shape.
    path.rLineTo(-kTriangleHeight, -kTriangleLength / 2);
    path.rLineTo(kTriangleHeight, -kTriangleLength / 2);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, -limit - action_offset);
  } else {
    // Bottom left after corner radius to top left corner radius.
    path.rLineTo(0, -short_height);
  }
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, kCornerRadius, -kCornerRadius);
  // Path finish.
  path.close();
  return path;
}

}  // namespace

ArrowContainer::ArrowContainer() {
  UpdateBorder();
}

ArrowContainer::~ArrowContainer() = default;

void ArrowContainer::SetArrowVerticalOffset(int offset) {
  if (arrow_vertical_offset_ != offset) {
    arrow_vertical_offset_ = offset;
    SchedulePaint();
  }
}

void ArrowContainer::SetArrowOnLeft(bool arrow_on_left) {
  if (arrow_on_left_ != arrow_on_left) {
    arrow_on_left_ = arrow_on_left;
    UpdateBorder();
    SchedulePaint();
  }
}

void ArrowContainer::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(
      arrow_on_left_
          ? gfx::Insets::TLBR(kBorderInset, kBorderInset + kTriangleHeight,
                              kBorderInset, kBorderInset)
          : gfx::Insets::TLBR(kBorderInset, kBorderInset, kBorderInset,
                              kBorderInset + kTriangleHeight)));
}

void ArrowContainer::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  // Draw the shape.
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  ui::ColorProvider* color_provider = GetColorProvider();
  flags.setColor(
      color_provider->GetColor(cros_tokens::kCrosSysSystemBaseElevatedOpaque));

  int height = GetHeightForWidth(kMenuWidth);
  canvas->DrawPath(
      BackgroundPath(height, arrow_on_left_, arrow_vertical_offset_), flags);
  // Draw the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(b/270969760): Change to "sys.BorderHighlight1" when added.
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysSystemBorder1));
  flags.setStrokeWidth(kBorderThickness);
  canvas->DrawPath(
      BackgroundPath(height, arrow_on_left_, arrow_vertical_offset_), flags);
}

gfx::Size ArrowContainer::CalculatePreferredSize() const {
  return gfx::Size(kMenuWidth, GetHeightForWidth(kMenuWidth));
}

}  // namespace arc::input_overlay
