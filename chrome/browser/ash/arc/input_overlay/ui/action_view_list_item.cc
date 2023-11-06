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

namespace arc::input_overlay {

ActionViewListItem::ActionViewListItem(DisplayOverlayController* controller,
                                       Action* action)
    : ActionEditView(controller,
                     action,
                     ash::RoundedContainer::Behavior::kAllRounded) {}

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
}

}  // namespace arc::input_overlay
