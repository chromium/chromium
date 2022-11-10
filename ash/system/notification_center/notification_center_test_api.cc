// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_test_api.h"

#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/stacked_notification_bar.h"

namespace ash {

NotificationCenterTestApi::NotificationCenterTestApi(
    NotificationCenterTray* tray)
    : notification_center_tray_(tray) {}

std::string NotificationCenterTestApi::AddNotification() {
  std::string id = base::NumberToString(notification_id_++);

  message_center::MessageCenter::Get()->AddNotification(
      CreateNotification(id, /*title=*/"test_title"));
  return id;
}

void NotificationCenterTestApi::RemoveNotification(const std::string& id) {
  message_center::MessageCenter::Get()->RemoveNotification(id,
                                                           /*by_user=*/true);
}

bool NotificationCenterTestApi::IsBubbleShown() {
  return notification_center_tray_->is_active() && GetWidget()->IsVisible();
}

bool NotificationCenterTestApi::IsTrayShown() {
  return notification_center_tray_->GetVisible();
}

NotificationCenterTray* NotificationCenterTestApi::GetTray() {
  return notification_center_tray_;
}

views::Widget* NotificationCenterTestApi::GetWidget() {
  return notification_center_tray_->GetBubbleWidget();
}

NotificationCenterBubble* NotificationCenterTestApi::GetBubble() {
  return notification_center_tray_->bubble_.get();
}

views::View* NotificationCenterTestApi::GetClearAllButton() {
  return notification_center_tray_->bubble_->notification_center_view_
      ->notification_bar_->clear_all_button_;
}

std::unique_ptr<message_center::Notification>
NotificationCenterTestApi::CreateNotification(const std::string& id,
                                              const std::string& title) {
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, base::UTF8ToUTF16(title),
      u"test message", ui::ImageModel(),
      /*display_source=*/std::u16string(), GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::NotificationDelegate());
}

}  // namespace ash