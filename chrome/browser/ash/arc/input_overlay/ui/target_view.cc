// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"

#include <algorithm>

#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

namespace {

constexpr int kDashedLineStrokeWidth = 1;
constexpr int kDashLength = 2;
constexpr int kDashGap = 4;

constexpr int kCircleRingStroke = 3;

constexpr int kActionTapCircleRadius = 37;
constexpr int kActionTapCircleRingRadius = 29;  // 32 - inside stroke 3

constexpr int kActionMoveCircleRadius = 69;
constexpr int kActionMoveCircleRingRadius = 61;  // 64 - inside stroke 3

// Returns true if `val` is inside of the circle with `radius` and `center`.
bool InsideOfCircle(const int radius, const int center, const int val) {
  return val >= center - radius && val < center + radius;
}

// Clamps `start` and `end` oustside of the circle with `radius` and `center`.
void ClampDashLineInCircle(const int radius,
                           const int center,
                           const int limit,
                           int& start,
                           int& end) {
  if (InsideOfCircle(radius, center, start) &&
      InsideOfCircle(radius, center, end)) {
    // Whole dash line segment of `kDashLength` is inside of the circle.
    start = end = 0;
  } else if (InsideOfCircle(radius, center, start)) {
    // `start` is inside of the circle.
    start = center + radius;
  } else if (InsideOfCircle(radius, center, end)) {
    // `end` is inside of the circle.
    end = center - radius;
  }

  start = std::clamp(start, 0, limit);
  end = std::clamp(end, 0, limit);
}

}  // namespace

TargetView::TargetView(DisplayOverlayController* controller,
                       ActionType action_type)
    : controller_(controller), action_type_(action_type) {
  const auto* touch_injector = controller_->touch_injector();
  DCHECK(touch_injector);
  const auto& bounds = touch_injector->content_bounds();
  center_.set_x(bounds.width() / 2);
  center_.set_y(bounds.height() / 2);
}

TargetView::~TargetView() = default;

void TargetView::UpdateWidgetBounds() {
  auto* widget = GetWidget();
  DCHECK(widget);

  widget->SetBounds(controller_->touch_injector()->content_bounds());
}

void TargetView::VisibilityChanged(views::View* starting_from,
                                   bool is_visible) {
  if (is_visible) {
    UpdateWidgetBounds();
  }
}

int TargetView::GetCircleRadius() {
  switch (action_type_) {
    case ActionType::TAP:
      return kActionTapCircleRadius;
    case ActionType::MOVE:
      return kActionMoveCircleRadius;
    default:
      NOTREACHED();
  }
}

int TargetView::GetCircleRingRadius() {
  switch (action_type_) {
    case ActionType::TAP:
      return kActionTapCircleRingRadius;
    case ActionType::MOVE:
      return kActionMoveCircleRingRadius;
    default:
      NOTREACHED();
  }
}

void TargetView::OnMouseMoved(const ui::MouseEvent& event) {
  center_ = event.location();
  SchedulePaint();
}

bool TargetView::OnMousePressed(const ui::MouseEvent& event) {
  // Considered as consumed. If the mouse is clicked, a new action is added when
  // the mouse button is released.
  return true;
}

void TargetView::OnMouseReleased(const ui::MouseEvent& event) {
  controller_->AddNewAction(action_type_, center_);
}

void TargetView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const auto* color_provider = GetColorProvider();

  const int circle_ring_radius = GetCircleRingRadius();
  const int circle_ring_overall_radius = circle_ring_radius + kCircleRingStroke;
  const int center_x = center_.x();
  const int center_y = center_.y();

  // Draw background circle.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(
      SkColorSetA(color_provider->GetColor(cros_tokens::kCrosSysPrimary),
                  GetAlpha(/*percent=*/0.8f)));
  canvas->DrawCircle(center_, GetCircleRadius(), flags);

  // Draw dashed crossed lines.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kDashedLineStrokeWidth);
  flags.setStrokeCap(cc::PaintFlags::Cap::kRound_Cap);
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysWhite));
  const auto& view_size = size();
  const int width = view_size.width();
  const int height = view_size.height();
  // Draw the horizontal dashed line.
  for (int x = 0; x < width; x += kDashGap + kDashLength) {
    int start = x;
    int end = x + kDashLength;
    ClampDashLineInCircle(circle_ring_overall_radius, center_x, width, start,
                          end);
    if (start < end) {
      canvas->DrawLine(gfx::Point(start, center_y), gfx::Point(end, center_y),
                       flags);
    }
  }
  // Draw the vertical dashed line.
  for (int y = 0; y < height; y += kDashGap + kDashLength) {
    int start = y;
    int end = y + kDashLength;
    ClampDashLineInCircle(circle_ring_overall_radius, center_y, height, start,
                          end);
    if (start < end) {
      canvas->DrawLine(gfx::Point(center_x, start), gfx::Point(center_x, end),
                       flags);
    }
  }

  // Draw the white circle ring.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysWhite));
  flags.setStrokeWidth(kCircleRingStroke);
  canvas->DrawCircle(center_, circle_ring_radius, flags);

  // Draw the touch point.
  TouchPoint::DrawTouchPoint(canvas, color_provider, action_type_,
                             UIState::kDefault, center_);
}

BEGIN_METADATA(TargetView, views::View)
END_METADATA
}  // namespace arc::input_overlay
