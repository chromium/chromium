// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/popups_only_ui_controller.h"

#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/desktop_message_popup_collection.h"

PopupsOnlyUiController::PopupsOnlyUiController()
    : message_center_(message_center::MessageCenter::Get()) {
  message_center_->AddObserver(this);
  message_center_->SetHasMessageCenterView(false);

  // Initialize collection after calling message_center_->AddObserver to ensure
  // the correct order of observers. (PopupsOnlyUiController has to be called
  // before MessagePopupCollection, see crbug.com/901350)
  popup_collection_ =
      std::make_unique<message_center::DesktopMessagePopupCollection>();
}

PopupsOnlyUiController::~PopupsOnlyUiController() {
  message_center_->RemoveObserver(this);
}

void PopupsOnlyUiController::OnNotificationAdded(
    const std::string& notification_id) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnNotificationUpdated(
    const std::string& notification_id) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnNotificationClicked(
    const std::string& notification_id,
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (popups_visible_)
    ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnBlockingStateChanged(
    message_center::NotificationBlocker* blocker) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::ShowOrHidePopupBubbles() {
  if (popups_visible_ && !message_center_->HasPopupNotifications()) {
    popups_visible_ = false;
  } else if (!popups_visible_ && message_center_->HasPopupNotifications()) {
    popup_collection_->StartObserving(display::Screen::GetScreen());
    popups_visible_ = true;
  }
}
