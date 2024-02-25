// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"

#include "ash/style/rounded_container.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace arc::input_overlay {

ActionViewListItem::ActionViewListItem(DisplayOverlayController* controller,
                                       Action* action)
    : ActionEditView(controller,
                     action,
                     /*is_editing_list=*/true) {}

ActionViewListItem::~ActionViewListItem() = default;

void ActionViewListItem::PerformPulseAnimation() {
  labels_view_->PerformPulseAnimationOnFirstLabel();
}

void ActionViewListItem::ClickCallback() {
  controller_->AddButtonOptionsMenuWidget(action_);
}

void ActionViewListItem::OnMouseEntered(const ui::MouseEvent& event) {
  controller_->AddActionHighlightWidget(action_);
  controller_->AddDeleteEditShortcutWidget(this);
}

void ActionViewListItem::OnMouseExited(const ui::MouseEvent& event) {
  controller_->HideActionHighlightWidget();
}

bool ActionViewListItem::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RIGHT) {
    controller_->AddDeleteEditShortcutWidget(this);
    return true;
  }

  // Don't hide the action view highlight because the focus may traverse inside
  // of this view. If the next focus view is not inside of this view, then hide
  // the action view highlight.
  if (views::FocusManager::IsTabTraversalKeyEvent(event)) {
    auto* focus_manager = GetFocusManager();
    if (auto* next_view = focus_manager->GetNextFocusableView(
            /*starting_view=*/focus_manager->GetFocusedView(),
            /*starting_widget=*/GetWidget(), /*reverse=*/event.IsShiftDown(),
            /*dont_loop=*/false);
        !next_view || !Contains(next_view)) {
      controller_->HideActionHighlightWidget();
    }
    // Tab key is not considered as processed here, so it falls to the end to
    // return false.
  }
  return false;
}

void ActionViewListItem::OnFocus() {
  controller_->AddActionHighlightWidget(action_);
}

BEGIN_METADATA(ActionViewListItem)
END_METADATA

}  // namespace arc::input_overlay
