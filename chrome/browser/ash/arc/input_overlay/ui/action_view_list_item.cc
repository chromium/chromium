// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
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

void ActionViewListItem::ClickCallback() {
  RecordEditingListFunctionTriggered(controller_->GetPackageName(),
                                     EditingListFunction::kPressListItem);
  controller_->AddButtonOptionsMenuWidget(action_);
}

void ActionViewListItem::OnMouseEntered(const ui::MouseEvent& event) {
  controller_->AddActionHighlightWidget(action_);
  controller_->AddDeleteEditShortcutWidget(this);
  RecordEditingListFunctionTriggered(controller_->GetPackageName(),
                                     EditingListFunction::kHoverListItem);
}

void ActionViewListItem::OnMouseExited(const ui::MouseEvent& event) {
  if (auto* focus_manager = GetFocusManager()) {
    auto* focused_view = focus_manager->GetFocusedView();
    // Hide the highlight when no view is focused or the focused view is not
    // this view or any view inside.
    if (!focused_view || !(focused_view == this || Contains(focused_view))) {
      controller_->HideActionHighlightWidgetForAction(action_);
    }
  }
}

bool ActionViewListItem::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RIGHT) {
    controller_->AddDeleteEditShortcutWidget(this);
    return true;
  }

  return ActionEditView::OnKeyPressed(event);
}

void ActionViewListItem::OnFocus() {
  controller_->AddActionHighlightWidget(action_);
}

void ActionViewListItem::OnBlur() {
  if (!IsMouseHovered()) {
    controller_->HideActionHighlightWidgetForAction(action_);
  }
}

BEGIN_METADATA(ActionViewListItem)
END_METADATA

}  // namespace arc::input_overlay
