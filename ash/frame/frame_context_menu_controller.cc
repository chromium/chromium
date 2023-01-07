// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/frame_context_menu_controller.h"

#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

FrameContextMenuController::FrameContextMenuController(views::Widget* frame,
                                                       Delegate* delegate)
    : frame_(frame), delegate_(delegate) {}

FrameContextMenuController::~FrameContextMenuController() = default;

void FrameContextMenuController::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (!chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
          frame_->GetNativeWindow())) {
    return;
  }

  DCHECK(delegate_);
  if (!delegate_->ShouldShowContextMenu(source, point))
    return;

  if (!move_to_desks_menu_model_) {
    move_to_desks_menu_model_ =
        std::make_unique<chromeos::MoveToDesksMenuModel>(
            std::make_unique<chromeos::MoveToDesksMenuDelegate>(frame_),
            /*add_title=*/true);
  }

  // Recreate the `menu_runner_` so the checked label of
  // `move_to_desks_menu_model_` will be updated.
  menu_runner_ = std::make_unique<views::MenuRunner>(
      move_to_desks_menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(frame_, /*button_controller=*/nullptr,
                          gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

}  // namespace ash
