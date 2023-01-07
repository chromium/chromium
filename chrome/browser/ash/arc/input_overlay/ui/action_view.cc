// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/strings/string_piece.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace arc {
namespace input_overlay {
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
      beta_(display_overlay_controller->touch_injector()->beta()) {}
ActionView::~ActionView() = default;

void ActionView::SetDisplayMode(DisplayMode mode, ActionLabel* editing_label) {
  DCHECK(mode != DisplayMode::kEducation && mode != DisplayMode::kMenu &&
         mode != DisplayMode::kPreMenu);
  if (mode == DisplayMode::kEducation || mode == DisplayMode::kMenu ||
      mode == DisplayMode::kPreMenu) {
    return;
  }

  if (!editable_ && mode == DisplayMode::kEdit)
    return;
  if (mode == DisplayMode::kView) {
    RemoveEditButton();
    if (!IsInputBound(action_->GetCurrentDisplayedInput()))
      SetVisible(false);
  }
  if (mode == DisplayMode::kEdit) {
    AddEditButton();
    if (!IsInputBound(*action_->current_input()))
      SetVisible(true);
  }

  if (show_circle() && circle_)
    circle_->SetDisplayMode(mode);
  if (!editing_label) {
    for (auto* label : labels_)
      label->SetDisplayMode(mode);
  } else {
    editing_label->SetDisplayMode(mode);
  }
}

void ActionView::SetPositionFromCenterPosition(
    const gfx::PointF& center_position) {
  int left = std::max(0, (int)(center_position.x() - center_.x()));
  int top = std::max(0, (int)(center_position.y() - center_.y()));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

gfx::Point ActionView::GetEditMenuPosition(gfx::Size menu_size) {
  DCHECK(menu_entry_);
  if (!menu_entry_)
    return gfx::Point();
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
    editing_label->GetViewAccessibility().OverrideDescription(
        base::UTF8ToUTF16(message));
  }
}

void ActionView::ShowInfoMsg(const base::StringPiece& message,
                             ActionLabel* editing_label) {
  display_overlay_controller_->AddEditMessage(message, MessageType::kInfo);
}

void ActionView::ShowLabelFocusInfoMsg(const base::StringPiece& message,
                                       ActionLabel* editing_label) {
  display_overlay_controller_->AddEditMessage(message,
                                              MessageType::kInfoLabelFocus);
  editing_label->GetViewAccessibility().OverrideDescription(
      base::UTF8ToUTF16(message));
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

bool ActionView::OnMousePressed(const ui::MouseEvent& event) {
  if (!beta_)
    return false;
  OnDragStart(event);
  return true;
}

bool ActionView::OnMouseDragged(const ui::MouseEvent& event) {
  return beta_ ? OnDragUpdate(event) : false;
}

void ActionView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!beta_)
    return;
  OnDragEnd();
}

void ActionView::OnGestureEvent(ui::GestureEvent* event) {
  if (!beta_)
    return;
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (OnDragUpdate(*event))
        event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      OnDragEnd();
      event->SetHandled();
      break;
    default:
      break;
  }
}

void ActionView::AddEditButton() {
  if (!show_edit_button_ || !editable_ || menu_entry_)
    return;

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
  if (!editable_ || !menu_entry_)
    return;
  RemoveChildViewT(menu_entry_);
  menu_entry_ = nullptr;
}

void ActionView::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_pos_ = event.location();
}

bool ActionView::OnDragUpdate(const ui::LocatedEvent& event) {
  auto new_location = event.location();
  auto target_location = origin() + (new_location - start_drag_pos_);
  target_location.set_x(base::clamp(target_location.x(), /*lo=*/0,
                                    /*hi=*/parent()->width() - width()));
  target_location.set_y(base::clamp(target_location.y(), /*lo=*/0,
                                    /*hi=*/parent()->height() - height()));
  SetPosition(target_location);
  return true;
}

void ActionView::OnDragEnd() {
  auto new_touch_center =
      gfx::Point(origin().x() + center_.x(), origin().y() + center_.y());
  ChangePositionBinding(new_touch_center);
}

void ActionView::ChangePositionBinding(const gfx::Point& new_touch_center) {
  DCHECK(beta_);
  if (!beta_)
    return;

  action_->PrepareToBindPosition(new_touch_center);
}

}  // namespace input_overlay
}  // namespace arc
