// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include <algorithm>

#include "ash/app_list/app_list_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/arrow_container.h"
#include "chrome/browser/ash/arc/input_overlay/ui/reposition_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"

namespace arc::input_overlay {

namespace {
constexpr int kAttachMargin = 8;
}

ActionView::ActionView(Action* action,
                       DisplayOverlayController* display_overlay_controller)
    : views::View(),
      action_(action),
      display_overlay_controller_(display_overlay_controller) {}
ActionView::~ActionView() = default;

void ActionView::OnActionInputBindingUpdated() {
  SetViewContent(BindingOption::kCurrent);
}

void ActionView::OnContentBoundsSizeChanged() {
  SetPositionFromCenterPosition(action_->GetUICenterPosition());
}

void ActionView::SetDisplayMode(DisplayMode mode, ActionLabel* editing_label) {
  DCHECK(mode != DisplayMode::kEducation && mode != DisplayMode::kMenu &&
         mode != DisplayMode::kPreMenu);
  if (mode == DisplayMode::kEducation || mode == DisplayMode::kMenu ||
      mode == DisplayMode::kPreMenu) {
    return;
  }

  // Set display mode for ActionLabel first and then other components update the
  // layout according to ActionLabel.
  if (!editing_label) {
    for (arc::input_overlay::ActionLabel* label : labels_) {
      label->SetDisplayMode(mode);
    }
  } else {
    editing_label->SetDisplayMode(mode);
  }

  if (mode == DisplayMode::kView) {
    display_mode_ = DisplayMode::kView;
    if (!IsInputBound(action_->GetCurrentDisplayedInput())) {
      SetVisible(false);
    }
    RemoveTouchPoint();
  }
  if (mode == DisplayMode::kEdit) {
    display_mode_ = DisplayMode::kEdit;
    AddTouchPoint();
    if (!IsInputBound(*action_->current_input())) {
      SetVisible(true);
    }
  }
}

void ActionView::SetPositionFromCenterPosition(
    const gfx::PointF& center_position) {
  DCHECK(touch_point_center_);
  const int left =
      std::max(0, (int)(center_position.x() - touch_point_center_->x()));
  const int top =
      std::max(0, (int)(center_position.y() - touch_point_center_->y()));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

void ActionView::ShowErrorMsg(std::string_view message,
                              ActionLabel* editing_label,
                              bool ax_annouce) {
  display_overlay_controller_->AddEditMessage(message, MessageType::kError);
  SetDisplayMode(DisplayMode::kEditedError, editing_label);
  if (ax_annouce) {
    GetViewAccessibility().AnnounceText(base::UTF8ToUTF16(message));
  } else {
    editing_label->GetViewAccessibility().SetDescription(
        base::UTF8ToUTF16(message));
  }
}

void ActionView::ShowInfoMsg(std::string_view message,
                             ActionLabel* editing_label) {
  display_overlay_controller_->AddEditMessage(message, MessageType::kInfo);
}

void ActionView::ShowFocusInfoMsg(std::string_view message, views::View* view) {
  display_overlay_controller_->AddEditMessage(message,
                                              MessageType::kInfoLabelFocus);
  view->GetViewAccessibility().SetDescription(base::UTF8ToUTF16(message));
}

void ActionView::RemoveMessage() {
  display_overlay_controller_->RemoveEditMessage();
}

void ActionView::ChangeInputBinding(
    Action* action,
    ActionLabel* action_label,
    std::unique_ptr<InputElement> input_element) {
  display_overlay_controller_->OnInputBindingChange(action,
                                                    std::move(input_element));
  SetDisplayMode(DisplayMode::kEditedSuccess, action_label);
}

void ActionView::OnResetBinding() {
  if (const auto& input_binding = action_->GetCurrentDisplayedInput();
      !IsInputBound(input_binding) ||
      input_binding == *action_->current_input()) {
    return;
  }

  auto input_element =
      std::make_unique<InputElement>(*(action_->current_input()));
  display_overlay_controller_->OnInputBindingChange(action_,
                                                    std::move(input_element));
}

bool ActionView::ShouldShowErrorMsg(ui::DomCode code,
                                    ActionLabel* editing_label) {
  if ((!action_->support_modifier_key() &&
       ModifierDomCodeToEventFlag(code) != ui::EF_NONE) ||
      IsReservedDomCode(code)) {
    ShowErrorMsg(l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_RESERVED_KEYS),
                 editing_label, /*ax_annouce=*/true);
    return true;
  }

  return false;
}

void ActionView::OnChildLabelUpdateFocus(ActionLabel* child, bool focus) {
  if (labels_.size() == 1u) {
    return;
  }

  for (arc::input_overlay::ActionLabel* label : labels_) {
    if (label == child) {
      continue;
    }
    label->OnSiblingUpdateFocus(focus);
  }
}

void ActionView::RemoveNewState() {
  for (arc::input_overlay::ActionLabel* label : labels_) {
    label->RemoveNewState();
  }
}

void ActionView::ApplyMousePressed(const ui::MouseEvent& event) {
  reposition_controller_->OnMousePressed(event);
}

void ActionView::ApplyMouseDragged(const ui::MouseEvent& event) {
  reposition_controller_->OnMouseDragged(event);
}

void ActionView::ApplyMouseReleased(const ui::MouseEvent& event) {
  if (!reposition_controller_->OnMouseReleased(event)) {
    ShowButtonOptionsMenu();
  }
}

void ActionView::ApplyGestureEvent(ui::GestureEvent* event) {
  if (!reposition_controller_->OnGestureEvent(event)) {
    ShowButtonOptionsMenu();
  }
}

bool ActionView::ApplyKeyPressed(const ui::KeyEvent& event) {
  return reposition_controller_->OnKeyPressed(event);
}

bool ActionView::ApplyKeyReleased(const ui::KeyEvent& event) {
  return reposition_controller_->OnKeyReleased(event);
}

void ActionView::ShowButtonOptionsMenu() {
  DCHECK(display_overlay_controller_);
  display_overlay_controller_->AddButtonOptionsMenuWidget(action_);
}

void ActionView::OnDraggingCallback() {
  MayUpdateLabelPosition();
  display_overlay_controller_->SetButtonOptionsMenuWidgetVisibility(
      /*is_visible=*/false);
}

void ActionView::OnMouseDragEndCallback() {
  action_->PrepareToBindPosition(GetTouchCenterInWindow());
  // "Restore to default" and "Cancel" functions are removed for Beta version,
  // so the position change is applied immediately after change.
  if (IsBeta()) {
    action_->BindPending();
  }

  display_overlay_controller_->SetButtonOptionsMenuWidgetVisibility(
      /*is_visible=*/true);

  RecordInputOverlayActionReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kMouseDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

void ActionView::OnGestureDragEndCallback() {
  action_->PrepareToBindPosition(GetTouchCenterInWindow());
  // "Restore to default" and "Cancel" functions are removed for Beta version,
  // so the position change is applied immediately after change.
  if (IsBeta()) {
    action_->BindPending();
  }

  display_overlay_controller_->SetButtonOptionsMenuWidgetVisibility(
      /*is_visible=*/true);

  RecordInputOverlayActionReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kTouchscreenDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

void ActionView::OnKeyPressedCallback() {
  MayUpdateLabelPosition();
}

void ActionView::OnKeyReleasedCallback() {
  action_->PrepareToBindPosition(GetTouchCenterInWindow());
  // "Restore to default" and "Cancel" functions are removed for Beta version,
  // so the position change is applied immediately after change.
  if (IsBeta()) {
    action_->BindPending();
  }
  RecordInputOverlayActionReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kKeyboardArrowKeyReposition,
      display_overlay_controller_->GetWindowStateType());
}

void ActionView::SetTouchPointCenter(const gfx::Point& touch_point_center) {
  touch_point_center_ = touch_point_center;
  if (touch_point_) {
    touch_point_->OnCenterPositionChanged(*touch_point_center_);
  }
}

void ActionView::AddTouchPoint(ActionType action_type) {
  if (touch_point_) {
    return;
  }

  DCHECK(touch_point_center_);
  touch_point_ = TouchPoint::Show(this, action_type, *touch_point_center_);
}

void ActionView::RemoveTouchPoint() {
  if (!touch_point_) {
    return;
  }

  RemoveChildViewT(touch_point_);
  touch_point_ = nullptr;
}

gfx::Point ActionView::GetTouchCenterInWindow() const {
  if (!touch_point_center_) {
    auto point = action_->GetUICenterPosition();
    return gfx::Point(point.x(), point.y());
  }

  auto pos = *touch_point_center_;
  pos.Offset(origin().x(), origin().y());
  return pos;
}

gfx::Point ActionView::CalculateAttachViewPositionInRootWindow(
    const gfx::Rect& available_bounds,
    const gfx::Point& window_content_origin,
    ArrowContainer* attached_view) const {
  auto origin_in_window = origin();
  origin_in_window.Offset(window_content_origin.x(), window_content_origin.y());

  // Check if `attached_view` can be placed on the left side or right side of
  // this view. It depends on if there is enough space in its own display. If
  // there is enough space on both sides, `can_attach_on_left` and
  // `can_attach_on_right` are true.
  bool can_attach_on_left = false, can_attach_on_right = false;

  const auto attached_view_size = attached_view->GetPreferredSize();
  // Width of `attached_view` including the margin of this view.
  const int attached_view_width_extra =
      kAttachMargin + attached_view_size.width();
  if (origin_in_window.x() + width() + attached_view_width_extra <=
      available_bounds.width()) {
    can_attach_on_right = true;
  }

  if (origin_in_window.x() - attached_view_width_extra >= 0) {
    can_attach_on_left = true;
  }

  // Calculate the position of x.
  int x = 0;
  const auto touch_center_in_window = GetTouchCenterInWindow();
  // If the display space is not considered, the position of `attached_view` is
  // toward to the center of the game window, which means if this view is on the
  // left of the window, then `attached_view` should be placed on the right side
  // of this view.
  bool should_attach_on_right =
      touch_center_in_window.x() < parent()->size().width() / 2.0;

  // `final_attach_on_left` is the final decision based on
  // `should_attach_on_right`, `can_attach_on_left` and `can_attach_on_right`.
  bool final_attach_on_left = false;
  if (should_attach_on_right) {
    if (!can_attach_on_right && can_attach_on_left) {
      // Attach `attached_view` on the left side of this view.
      x = origin_in_window.x() - attached_view_width_extra;
      final_attach_on_left = true;
    } else {
      // Attach `attached_view` on the right side of this view.
      x = origin_in_window.x() + width() + kAttachMargin;
      if (x + attached_view_size.width() > available_bounds.width()) {
        x = available_bounds.width() - attached_view_size.width();
      }
    }
  } else {
    if (!can_attach_on_left && can_attach_on_right) {
      // Attach `attached_view` on the right side of this view.
      x = origin_in_window.x() + width() + kAttachMargin;
    } else {
      // Attach `attached_view` on the left side of this view.
      x = std::max(0, origin_in_window.x() - attached_view_width_extra);
      final_attach_on_left = true;
    }
  }

  attached_view->SetArrowOnLeft(!final_attach_on_left);

  // Check y position to make sure that `attached_view` shows completely inside
  // of the display.
  int y = std::max(0, window_content_origin.y() + touch_center_in_window.y() -
                          attached_view_size.height() / 2);
  y = std::min(y, available_bounds.height() - attached_view_size.height());
  attached_view->SetArrowVerticalOffset(
      touch_center_in_window.y() -
      (y - window_content_origin.y() + attached_view_size.height() / 2));
  return gfx::Point(x, y);
}

void ActionView::AddedToWidget() {
  SetRepositionController();
}

void ActionView::SetRepositionController() {
  if (reposition_controller_) {
    return;
  }
  reposition_controller_ = std::make_unique<RepositionController>(this);
  reposition_controller_->set_dragging_callback(base::BindRepeating(
      &ActionView::OnDraggingCallback, base::Unretained(this)));
  reposition_controller_->set_mouse_drag_end_callback(base::BindRepeating(
      &ActionView::OnMouseDragEndCallback, base::Unretained(this)));
  reposition_controller_->set_gesture_drag_end_callback(base::BindRepeating(
      &ActionView::OnGestureDragEndCallback, base::Unretained(this)));
  reposition_controller_->set_key_pressed_callback(base::BindRepeating(
      &ActionView::OnKeyPressedCallback, base::Unretained(this)));
  reposition_controller_->set_key_released_callback(base::BindRepeating(
      &ActionView::OnKeyReleasedCallback, base::Unretained(this)));
}

BEGIN_METADATA(ActionView)
END_METADATA

}  // namespace arc::input_overlay
