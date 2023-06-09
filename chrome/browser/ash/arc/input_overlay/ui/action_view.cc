// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include <algorithm>

#include "ash/app_list/app_list_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button_factory.h"

namespace arc::input_overlay {

ActionView::ActionView(Action* action,
                       DisplayOverlayController* display_overlay_controller)
    : views::View(),
      action_(action),
      display_overlay_controller_(display_overlay_controller),
      beta_(display_overlay_controller->touch_injector()->beta()) {}
ActionView::~ActionView() = default;

void ActionView::OnActionUpdated() {
  SetViewContent(BindingOption::kCurrent);
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
    for (auto* label : labels_)
      label->SetDisplayMode(mode);
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
  int left = std::max(0, (int)(center_position.x() - touch_point_center_->x()));
  int top = std::max(0, (int)(center_position.y() - touch_point_center_->y()));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

void ActionView::ShowErrorMsg(const base::StringPiece& message,
                              ActionLabel* editing_label,
                              bool ax_annouce) {
  display_overlay_controller_->AddEditMessage(message, MessageType::kError);
  SetDisplayMode(DisplayMode::kEditedError, editing_label);
  if (ax_annouce) {
    GetViewAccessibility().AnnounceText(base::UTF8ToUTF16(message));
  } else {
    editing_label->SetAccessibleDescription(base::UTF8ToUTF16(message));
  }
}

void ActionView::ShowInfoMsg(const base::StringPiece& message,
                             ActionLabel* editing_label) {
  display_overlay_controller_->AddEditMessage(message, MessageType::kInfo);
}

void ActionView::ShowFocusInfoMsg(const base::StringPiece& message,
                                  views::View* view) {
  display_overlay_controller_->AddEditMessage(message,
                                              MessageType::kInfoLabelFocus);
  view->SetAccessibleDescription(base::UTF8ToUTF16(message));
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
  const auto& input_binding = action_->GetCurrentDisplayedInput();
  if (!IsInputBound(input_binding) ||
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

  for (auto* label : labels_) {
    if (label == child) {
      continue;
    }
    label->OnSiblingUpdateFocus(focus);
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

void ActionView::OnDraggingCallback() {
  MayUpdateLabelPosition();
}

void ActionView::OnMouseDragEndCallback() {
  action_->PrepareToBindPosition(GetTouchCenterInWindow());
  RecordInputOverlayActionReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kMouseDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

void ActionView::OnGestureDragEndCallback() {
  action_->PrepareToBindPosition(GetTouchCenterInWindow());
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

void ActionView::ShowButtonOptionsMenu() {
  DCHECK(display_overlay_controller_);
  display_overlay_controller_->AddButtonOptionsMenu(action_);
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

}  // namespace arc::input_overlay
