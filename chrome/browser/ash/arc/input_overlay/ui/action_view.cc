// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include "ash/app_list/app_list_util.h"
#include "base/cxx17_backports.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button_factory.h"

namespace arc::input_overlay {
namespace {
constexpr int kMenuEntryOffset = 4;

// For the keys that are caught by display overlay, check if they are reserved
// for special use.
bool IsReservedDomCode(ui::DomCode code) {
  switch (code) {
    // Audio, brightness key events won't be caught by display overlay so no
    // need to add them.
    // Used for mouse lock.
    case ui::DomCode::ESCAPE:
    // Used for traversing the views, which is also required by Accessibility.
    case ui::DomCode::TAB:
    // Don't support according to UX requirement.
    case ui::DomCode::BROWSER_BACK:
    case ui::DomCode::BROWSER_FORWARD:
    case ui::DomCode::BROWSER_REFRESH:
      return true;
    default:
      return false;
  }
}

}  // namespace

ActionView::ActionView(Action* action,
                       DisplayOverlayController* display_overlay_controller)
    : views::View(),
      action_(action),
      display_overlay_controller_(display_overlay_controller),
      allow_reposition_(
          display_overlay_controller->touch_injector()->allow_reposition()),
      beta_(display_overlay_controller->touch_injector()->beta()) {}
ActionView::~ActionView() = default;

void ActionView::SetDisplayMode(DisplayMode mode, ActionLabel* editing_label) {
  DCHECK(mode != DisplayMode::kEducation && mode != DisplayMode::kMenu &&
         mode != DisplayMode::kPreMenu);
  if (mode == DisplayMode::kEducation || mode == DisplayMode::kMenu ||
      mode == DisplayMode::kPreMenu) {
    return;
  }

  if (!editable_ && mode == DisplayMode::kEdit) {
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
    RemoveEditButton();
    RemoveTrashButton();
    if (!IsInputBound(action_->GetCurrentDisplayedInput())) {
      SetVisible(false);
    }
    if (allow_reposition_) {
      RemoveTouchPoint();
    }
  }
  if (mode == DisplayMode::kEdit) {
    display_mode_ = DisplayMode::kEdit;
    if (allow_reposition_) {
      AddTouchPoint();
    }
    if (!IsInputBound(*action_->current_input())) {
      SetVisible(true);
    }
    AddEditButton();
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

gfx::Point ActionView::GetEditMenuPosition(gfx::Size menu_size) {
  DCHECK(menu_entry_);
  if (!menu_entry_) {
    return gfx::Point();
  }
  int x = action_->on_left_or_middle_side()
              ? bounds().x()
              : std::max(0, bounds().right() - menu_size.width());
  int y = bounds().y() <= menu_size.height()
              ? bounds().bottom()
              : bounds().y() - menu_size.height();
  return gfx::Point(x, y);
}

void ActionView::RemoveEditMenu() {
  display_overlay_controller_->RemoveActionEditMenu();
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

bool ActionView::ApplyMousePressed(const ui::MouseEvent& event) {
  if (!allow_reposition_) {
    return false;
  }
  OnDragStart(event);
  return true;
}

bool ActionView::ApplyMouseDragged(const ui::MouseEvent& event) {
  return allow_reposition_ ? OnDragUpdate(event) : false;
}

void ActionView::ApplyMouseReleased(const ui::MouseEvent& event) {
  if (!allow_reposition_) {
    return;
  }
  OnDragEnd();
  RecordInputOverlayActionReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kMouseDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

void ActionView::ApplyGestureEvent(ui::GestureEvent* event) {
  if (!allow_reposition_) {
    return;
  }
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (OnDragUpdate(*event)) {
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      OnDragEnd();
      event->SetHandled();
      RecordInputOverlayActionReposition(
          display_overlay_controller_->GetPackageName(),
          RepositionType::kTouchscreenDragRepostion,
          display_overlay_controller_->GetWindowStateType());
      break;
    default:
      break;
  }
}

bool ActionView::ApplyKeyPressed(const ui::KeyEvent& event) {
  auto target_location = origin();
  if (!allow_reposition_ ||
      !UpdatePositionByArrowKey(event.key_code(), target_location)) {
    return View::OnKeyPressed(event);
  }
  ClampPosition(target_location, size(), parent()->size());
  SetPosition(target_location);
  MayUpdateLabelPosition();
  return true;
}

bool ActionView::ApplyKeyReleased(const ui::KeyEvent& event) {
  if (!allow_reposition_ || !ash::IsArrowKeyEvent(event)) {
    return View::OnKeyReleased(event);
  }
  DCHECK(touch_point_center_);
  ChangePositionBinding(gfx::Point(origin().x() + touch_point_center_->x(),
                                   origin().y() + touch_point_center_->y()));
  RecordInputOverlayActionReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kKeyboardArrowKeyReposition,
      display_overlay_controller_->GetWindowStateType());
  return true;
}

void ActionView::SetTouchPointCenter(const gfx::Point& touch_point_center) {
  touch_point_center_ = touch_point_center;
  if (touch_point_) {
    touch_point_->OnCenterPositionChanged(*touch_point_center_);
  }
}

void ActionView::AddEditButton() {
  if (!show_edit_button_ || !editable_ || menu_entry_) {
    return;
  }

  menu_entry_ =
      AddChildView(std::make_unique<ActionEditButton>(base::BindRepeating(
          &ActionView::OnMenuEntryPressed, base::Unretained(this))));
  if (action_->on_left_or_middle_side()) {
    menu_entry_->SetPosition(gfx::Point(0, kMenuEntryOffset));
  } else {
    menu_entry_->SetPosition(gfx::Point(
        std::max(0, width() - menu_entry_->width()), kMenuEntryOffset));
  }
}

void ActionView::RemoveEditButton() {
  if (!editable_ || !menu_entry_) {
    return;
  }
  RemoveChildViewT(menu_entry_);
  menu_entry_ = nullptr;
}

void ActionView::RemoveTrashButton() {
  if (!editable_ || !trash_button_) {
    return;
  }

  RemoveChildViewT(trash_button_);
  trash_button_ = nullptr;
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

void ActionView::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
  ResetFocusTo(this);
}

bool ActionView::OnDragUpdate(const ui::LocatedEvent& event) {
  auto new_location = event.location();
  auto target_location = origin() + (new_location - start_drag_event_pos_);
  ClampPosition(target_location, size(), parent()->size());
  SetPosition(target_location);
  MayUpdateLabelPosition();
  return true;
}

void ActionView::OnDragEnd() {
  ChangePositionBinding(GetTouchCenterInWindow());
}

void ActionView::ChangePositionBinding(const gfx::Point& new_touch_center) {
  DCHECK(allow_reposition_);
  if (!allow_reposition_) {
    return;
  }

  action_->PrepareToBindPosition(new_touch_center);
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

}  // namespace arc::input_overlay
