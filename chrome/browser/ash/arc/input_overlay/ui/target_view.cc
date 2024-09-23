// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"

#include <algorithm>

#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_PLACEMENT_A11Y_LABEL));
}

TargetView::~TargetView() = default;

void TargetView::UpdateWidgetBounds() {
  auto* widget = GetWidget();
  DCHECK(widget);

  controller_->UpdateWidgetBoundsInRootWindow(
      widget, controller_->touch_injector()->content_bounds());
}

gfx::Rect TargetView::GetTargetCircleBounds() const {
  gfx::Rect bounds = gfx::Rect(center_.x(), center_.y(), 0, 0);
  bounds.Outset(GetCircleRadius());
  return bounds;
}

int TargetView::GetCircleRadius() const {
  switch (action_type_) {
    case ActionType::TAP:
      return kActionTapCircleRadius;
    case ActionType::MOVE:
      return kActionMoveCircleRadius;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

int TargetView::GetCircleRingRadius() const {
  switch (action_type_) {
    case ActionType::TAP:
      return kActionTapCircleRingRadius;
    case ActionType::MOVE:
      return kActionMoveCircleRingRadius;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

int TargetView::GetPadding() const {
  return TouchPoint::GetEdgeLength(action_type_) / 2;
}

void TargetView::ClampCenter() {
  const int padding = GetPadding();
  const auto& view_size = size();
  center_.set_x(std::clamp(center_.x(), /*lo=*/padding,
                           /*hi=*/view_size.width() - padding));
  center_.set_y(std::clamp(center_.y(), /*lo=*/padding,
                           /*hi=*/view_size.height() - padding));
}

void TargetView::OnCenterChanged() {
  ClampCenter();
  SchedulePaint();
  controller_->UpdateButtonPlacementNudgeAnchorRect();
}

void TargetView::MoveCursorToViewCenter() {
  auto* widget = GetWidget();
  DCHECK(widget);
  auto* window = widget->GetNativeWindow();
  DCHECK(window);
  window->MoveCursorTo(bounds().CenterPoint());
}

void TargetView::VisibilityChanged(views::View* starting_from,
                                   bool is_visible) {
  if (is_visible) {
    UpdateWidgetBounds();
    RequestFocus();
    MoveCursorToViewCenter();
  }
}

void TargetView::OnGestureEvent(ui::GestureEvent* event) {
  auto type = event->type();
  center_ = event->location();
  OnCenterChanged();
  event->SetHandled();

  // When the gesture event is released, add a new action.
  if (type == ui::EventType::kGestureScrollEnd ||
      type == ui::EventType::kScrollFlingStart ||
      type == ui::EventType::kGesturePinchEnd ||
      type == ui::EventType::kGestureEnd) {
    controller_->AddNewAction(action_type_, center_);
  }
}

bool TargetView::OnKeyPressed(const ui::KeyEvent& event) {
  const auto key_code = event.key_code();

  // Exit the button placement mode when key `esc` is pressed.
  if (key_code == ui::VKEY_ESCAPE) {
    controller_->ExitButtonPlaceMode(/*is_action_added=*/false);
    return true;
  }

  // Continue to add new action if key `enter` is pressed.
  if (key_code == ui::VKEY_RETURN) {
    controller_->AddNewAction(action_type_, center_);
    return true;
  }

  // Update `center_` and repaint if arrow keys are pressed.
  if (OffsetPositionByArrowKey(key_code, center_)) {
    OnCenterChanged();
    return true;
  }

  return false;
}

void TargetView::OnMouseMoved(const ui::MouseEvent& event) {
  center_ = event.location();
  OnCenterChanged();
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

void TargetView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // Repaint to update colors.
  SchedulePaint();
}

BEGIN_METADATA(TargetView)
END_METADATA
}  // namespace arc::input_overlay
