// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/arrow_container.h"

#include <cmath>

#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"

namespace arc::input_overlay {
namespace {

constexpr SkScalar kTriangleLength = 20.0f;
constexpr SkScalar kTriangleHeight = 14.0f;
// The straight distance from triangle rounded corner start to end.
constexpr SkScalar kTriangleRoundDistance = 4.0f;
constexpr SkScalar kCornerRadius = 16.0f;
constexpr SkScalar kBorderThickness = 2.0f;

// Whole menu width with arrow.
constexpr int kMenuWidth = kButtonOptionsMenuWidth + kTriangleHeight;

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
SkPath BackgroundPath(SkScalar height,
                      SkScalar action_offset,
                      bool draw_triangle_on_left) {
  SkPath path;
  const SkScalar short_length =
      SkIntToScalar(kMenuWidth) - kTriangleHeight - 2 * kCornerRadius;
  const SkScalar short_height = height - 2 * kCornerRadius;

  // Calculate values for drawing triangle rounded corner. Check b/324940844 for
  // calculation details.
  const SkScalar triangle_radius =
      kTriangleRoundDistance / 4 *
      std::sqrt(4 +
                std::pow(kTriangleLength, 2) / std::pow(kTriangleHeight, 2));
  const SkScalar dx =
      kTriangleHeight * kTriangleRoundDistance / kTriangleLength;
  const SkScalar dy = kTriangleRoundDistance / 2;

  // If the offset is greater than the limit or less than the negative
  // limit, set it respectively.
  const SkScalar limit = short_height / 2 - kTriangleLength / 2;
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
    path.rLineTo(kTriangleHeight - dx, kTriangleLength / 2 - dy);
    // Draw triangle rounded corner.
    path.rArcTo(triangle_radius, triangle_radius, 0, SkPath::kSmall_ArcSize,
                SkPathDirection::kCW, 0, kTriangleRoundDistance);
    path.rLineTo(-kTriangleHeight + dx, kTriangleLength / 2 - dy);
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
    path.rLineTo(-kTriangleHeight + dx, -kTriangleLength / 2 + dy);
    // Draw triangle rounded corner.
    path.rArcTo(triangle_radius, triangle_radius, 0, SkPath::kSmall_ArcSize,
                SkPathDirection::kCW, 0, -kTriangleRoundDistance);
    path.rLineTo(kTriangleHeight - dx, -kTriangleLength / 2 + dy);
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
      arrow_on_left_ ? gfx::Insets::TLBR(kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset +
                                             kTriangleHeight,
                                         kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset)
                     : gfx::Insets::TLBR(kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset +
                                             kTriangleHeight)));
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
      BackgroundPath(SkIntToScalar(height),
                     SkIntToScalar(arrow_vertical_offset_), arrow_on_left_),
      flags);
  // Draw the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(b/270969760): Change to "sys.BorderHighlight1" when added.
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysSystemBorder1));
  flags.setStrokeWidth(kBorderThickness);
  canvas->DrawPath(
      BackgroundPath(SkIntToScalar(height),
                     SkIntToScalar(arrow_vertical_offset_), arrow_on_left_),
      flags);
}

gfx::Size ArrowContainer::CalculatePreferredSize() const {
  return gfx::Size(kMenuWidth, GetHeightForWidth(kMenuWidth));
}

BEGIN_METADATA(ArrowContainer)
END_METADATA

}  // namespace arc::input_overlay
