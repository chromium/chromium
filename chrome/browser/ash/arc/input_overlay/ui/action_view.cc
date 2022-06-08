// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include "base/bind.h"

namespace arc {
namespace input_overlay {
namespace {
constexpr int kMenuEntryOffset = 4;

// UI strings.
// TODO(cuicuiruan): move the strings to chrome/app/generated_resources.grd
// after UX/UI strings are confirmed.
constexpr base::StringPiece kEditErrorUnsupportedKey("Unsupported key");
constexpr base::StringPiece kEditErrorDuplicatedKey(
    "Duplicated key in the same action");

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
      display_overlay_controller_(display_overlay_controller) {}
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
    if (!IsBound(action_->GetCurrentDisplayedBinding()))
      SetVisible(false);
  }
  if (mode == DisplayMode::kEdit) {
    AddEditButton();
    if (!IsBound(*action_->current_binding()))
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

void ActionView::SetPositionFromCenterPosition(gfx::PointF& center_position) {
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
                              ActionLabel* editing_label) {
  display_overlay_controller_->AddEditMessage(this, message,
                                              MessageType::kError);
  SetDisplayMode(DisplayMode::kEditedError, editing_label);
}

void ActionView::ShowInfoMsg(const base::StringPiece& message,
                             ActionLabel* editing_label) {
  display_overlay_controller_->AddEditMessage(this, message,
                                              MessageType::kInfo);
}

void ActionView::RemoveMessage() {
  display_overlay_controller_->RemoveEditMessage();
}

void ActionView::ChangeBinding(Action* action,
                               ActionLabel* action_label,
                               std::unique_ptr<InputElement> input_element) {
  display_overlay_controller_->OnBindingChange(action,
                                               std::move(input_element));
  SetDisplayMode(DisplayMode::kEditedSuccess, action_label);
}

void ActionView::OnResetBinding() {
  const auto& binding = action_->GetCurrentDisplayedBinding();
  if (!IsBound(binding) || binding == *action_->current_binding())
    return;

  auto input_element =
      std::make_unique<InputElement>(*(action_->current_binding()));
  display_overlay_controller_->OnBindingChange(action_,
                                               std::move(input_element));
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

bool ActionView::ShouldShowErrorMsg(ui::DomCode code,
                                    ActionLabel* editing_label) {
  // Check if |code| is duplicated with the keys in its action. For example,
  // there are four keys involved in the key-bound |ActionMove|.
  auto& binding = action_->GetCurrentDisplayedBinding();
  if (IsKeyboardBound(binding)) {
    for (const auto& key : binding.keys()) {
      if (key != code)
        continue;
      ShowErrorMsg(kEditErrorDuplicatedKey, editing_label);
      return true;
    }
  }

  if ((!action_->support_modifier_key() &&
       ModifierDomCodeToEventFlag(code) != ui::EF_NONE) ||
      IsReservedDomCode(code)) {
    ShowErrorMsg(kEditErrorUnsupportedKey, editing_label);
    return true;
  }

  return false;
}

}  // namespace input_overlay
}  // namespace arc
