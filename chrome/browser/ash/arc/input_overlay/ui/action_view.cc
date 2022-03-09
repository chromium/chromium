// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include "base/bind.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

namespace arc {
namespace input_overlay {
namespace {
constexpr int kMenuEntryOffset = 4;
}

ActionView::ActionView(Action* action,
                       DisplayOverlayController* display_overlay_controller)
    : views::View(),
      action_(action),
      display_overlay_controller_(display_overlay_controller) {}
ActionView::~ActionView() = default;

void ActionView::SetDisplayMode(DisplayMode mode) {
  if ((!editable_ && mode == DisplayMode::kEdit) || mode == DisplayMode::kMenu)
    return;
  if (mode == DisplayMode::kView) {
    RemoveEditButton();
    if (menu_entry_) {
      RemoveChildViewT(menu_entry_);
      menu_entry_ = nullptr;
    }
  }
  if (mode == DisplayMode::kEdit) {
    AddEditButton();
    if (circle_)
      circle_->SetDisplayMode(mode);
    for (auto* label : labels_)
      label->SetDisplayMode(mode);
  }
}

void ActionView::SetPositionFromCenterPosition(gfx::PointF& center_position) {
  int left = std::max(0, (int)(center_position.x() - center_.x()));
  int top = std::max(0, (int)(center_position.y() - center_.y()));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

void ActionView::OnMenuEntryPressed() {
  display_overlay_controller_->AddActionEditMenu(this);
  DCHECK(menu_entry_);
  if (!menu_entry_)
    return;
  menu_entry_->RequestFocus();
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

void ActionView::AddEditButton() {
  if (!editable_ || menu_entry_)
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

}  // namespace input_overlay
}  // namespace arc
