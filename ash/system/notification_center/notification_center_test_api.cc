// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/base/models/image_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_popup_view.h"

namespace ash {

NotificationCenterTestApi::NotificationCenterTestApi(
    NotificationCenterTray* tray)
    : notification_center_tray_(tray) {}

void NotificationCenterTestApi::ToggleBubble() {
  auto event_generator =
      std::make_unique<ui::test::EventGenerator>(Shell::GetPrimaryRootWindow());

  gfx::Point click_location =
      notification_center_tray_
          ? notification_center_tray_->GetBoundsInScreen().CenterPoint()
          : Shell::Get()
                ->GetPrimaryRootWindowController()
                ->shelf()
                ->status_area_widget()
                ->unified_system_tray()
                ->GetBoundsInScreen()
                .CenterPoint();

  event_generator->MoveMouseTo(click_location);
  event_generator->ClickLeftButton();
}

std::string NotificationCenterTestApi::AddCustomNotification(
    const std::u16string& title,
    const std::u16string& message,
    const ui::ImageModel& icon,
    const std::u16string& display_source,
    const GURL& url,
    const message_center::NotifierId& notifier_id) {
  const std::string id = GenerateNotificationId();

  message_center::MessageCenter::Get()->AddNotification(CreateNotification(
      id, title, message, icon, display_source, url, notifier_id));
  return id;
}

std::string NotificationCenterTestApi::AddNotification() {
  return AddCustomNotification(/*title=*/u"test_title",
                               /*message=*/u"test_message");
}

std::string NotificationCenterTestApi::AddNotificationWithSourceUrl(
    const std::string& url) {
  const std::string id = GenerateNotificationId();

  GURL gurl = GURL(url);
  message_center::MessageCenter::Get()->AddNotification(CreateNotification(
      id, u"test_title", u"test_message", ui::ImageModel(),
      base::EmptyString16(), gurl, message_center::NotifierId(gurl)));

  return id;
}

void NotificationCenterTestApi::RemoveNotification(const std::string& id) {
  message_center::MessageCenter::Get()->RemoveNotification(id,
                                                           /*by_user=*/true);
}

size_t NotificationCenterTestApi::GetNotificationCount() const {
  return message_center::MessageCenter::Get()->NotificationCount();
}

bool NotificationCenterTestApi::IsBubbleShown() {
  return notification_center_tray_->is_active() && GetWidget()->IsVisible();
}

bool NotificationCenterTestApi::IsPopupShown(const std::string& id) {
  return message_center::MessageCenter::Get()->FindPopupNotificationById(id);
}

bool NotificationCenterTestApi::IsTrayShown() {
  return notification_center_tray_->GetVisible();
}

bool NotificationCenterTestApi::IsDoNotDisturbIconShown() {
  return notification_center_tray_->notification_icons_controller_
      ->quiet_mode_view()
      ->GetVisible();
}

message_center::MessageView*
NotificationCenterTestApi::GetNotificationViewForId(const std::string& id) {
  // Ensure this api is only called when the notification list view exists, i.e.
  // The notification center bubble is open.
  DCHECK(GetNotificationListView());

  return GetNotificationListView()->GetMessageViewForNotificationId(id);
}

views::View* NotificationCenterTestApi::GetPopupViewForId(
    const std::string& id) {
  // TODO(b/259459804): Move `MessagePopupCollection` to be owned by
  // `NotificationCenterTray` instead of `UnifiedSystemTray`.
  return Shell::Get()
      ->GetPrimaryRootWindowController()
      ->shelf()
      ->GetStatusAreaWidget()
      ->unified_system_tray()
      ->GetMessagePopupCollection()
      ->GetPopupViewForNotificationID(id);
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

views::View* NotificationCenterTestApi::GetNotificationCenterView() {
  return notification_center_tray_->bubble_->notification_center_view_;
}

views::View* NotificationCenterTestApi::GetClearAllButton() {
  return notification_center_tray_->bubble_->notification_center_view_
      ->notification_bar_->clear_all_button_;
}

std::string NotificationCenterTestApi::NotificationIdToParentNotificationId(
    const std::string& id) {
  return id + message_center::kIdSuffixForGroupContainerNotification;
}

std::string NotificationCenterTestApi::GenerateNotificationId() {
  return base::NumberToString(notification_id_++);
}

NotificationListView* NotificationCenterTestApi::GetNotificationListView() {
  DCHECK(message_center::MessageCenter::Get()->IsMessageCenterVisible());

  if (notification_center_tray_) {
    return notification_center_tray_->bubble_->notification_center_view_
        ->notification_list_view();
  }

  auto* unified_system_tray = Shell::Get()
                                  ->GetPrimaryRootWindowController()
                                  ->shelf()
                                  ->GetStatusAreaWidget()
                                  ->unified_system_tray();

  return unified_system_tray->message_center_bubble()
      ->notification_center_view()
      ->notification_list_view();
}

std::unique_ptr<message_center::Notification>
NotificationCenterTestApi::CreateNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const ui::ImageModel& icon,
    const std::u16string& display_source,
    const GURL& url,
    const message_center::NotifierId& notifier_id) {
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message, icon,
      display_source, url, notifier_id, message_center::RichNotificationData(),
      new message_center::NotificationDelegate());
}

}  // namespace ash
