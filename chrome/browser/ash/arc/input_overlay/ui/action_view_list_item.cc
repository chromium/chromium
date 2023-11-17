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

void ActionViewListItem::OnActionNameUpdated() {
  NOTIMPLEMENTED();
}

void ActionViewListItem::ClickCallback() {
  controller_->AddButtonOptionsMenuWidget(action_);
}

void ActionViewListItem::ShowEduNudgeForEditingTip() {
  labels_view_->ShowEduNudgeForEditingTip();
}

void ActionViewListItem::OnMouseEntered(const ui::MouseEvent& event) {
  controller_->AddDeleteEditShortcutWidget(this);
  controller_->AddActionHighlightWidget(action_);
}

void ActionViewListItem::OnMouseExited(const ui::MouseEvent& event) {
  controller_->HideActionHighlightWidget();
}

BEGIN_METADATA(ActionViewListItem)
END_METADATA

}  // namespace arc::input_overlay
