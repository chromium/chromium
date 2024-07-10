// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_test_api.h"

#include <cstdint>

#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

NotificationCenterTestApi::NotificationCenterTestApi()
    : primary_display_id_(
          display::Screen::GetScreen()->GetPrimaryDisplay().id()) {}

void NotificationCenterTestApi::ToggleBubble() {
  ToggleBubbleOnDisplay(primary_display_id_);
}

void NotificationCenterTestApi::ToggleBubbleOnDisplay(int64_t display_id) {
  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      Shell::GetRootWindowForDisplayId(display_id));

  gfx::Point click_location =
      GetTrayOnDisplay(display_id)->GetBoundsInScreen().CenterPoint();

  event_generator->MoveMouseTo(click_location);
  event_generator->ClickLeftButton();
}

std::string NotificationCenterTestApi::AddCustomNotification(
    const std::u16string& title,
    const std::u16string& message,
    const ui::ImageModel& icon,
    const std::u16string& display_source,
    const GURL& url,
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields) {
  const std::string id = GenerateNotificationId();

  auto notification =
      CreateNotification(id, title, message, icon, display_source, url,
                         notifier_id, optional_fields);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  return id;
}

std::string NotificationCenterTestApi::AddNotification() {
  return AddCustomNotification(/*title=*/u"test_title",
                               /*message=*/u"test_message");
}

std::string NotificationCenterTestApi::AddPinnedNotification() {
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  return AddCustomNotification(
      /*title=*/u"test_title",
      /*message=*/u"test_message", ui::ImageModel(), std::u16string(), GURL(),
      message_center::NotifierId(), optional_fields);
}

std::string NotificationCenterTestApi::AddNotificationWithSourceUrl(
    const std::string& url) {
  const std::string id = GenerateNotificationId();

  GURL gurl = GURL(url);
  message_center::MessageCenter::Get()->AddNotification(CreateNotification(
      id, u"test_title", u"test_message", ui::ImageModel(), std::u16string(),
      gurl, message_center::NotifierId(gurl),
      message_center::RichNotificationData()));

  return id;
}

std::string NotificationCenterTestApi::AddPinnedNotificationWithSourceUrl(
    const std::string& url) {
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  GURL gurl = GURL(url);
  return AddCustomNotification(
      /*title=*/u"test_title",
      /*message=*/u"test_message", ui::ImageModel(), std::u16string(), gurl,
      message_center::NotifierId(gurl), optional_fields);
}

std::string NotificationCenterTestApi::AddSystemNotification() {
  message_center::NotifierId notifier_id;
  notifier_id.type = message_center::NotifierType::SYSTEM_COMPONENT;
  message_center::RichNotificationData optional_fields;
  optional_fields.priority =
      message_center::NotificationPriority::SYSTEM_PRIORITY;
  return AddCustomNotification(
      /*title=*/u"test_title",
      /*message=*/u"test_message", ui::ImageModel(), std::u16string(), GURL(),
      notifier_id, optional_fields);
}

std::string NotificationCenterTestApi::AddCriticalWarningSystemNotification() {
  const auto id = GenerateNotificationId();
  message_center::NotifierId notifier_id;
  notifier_id.type = message_center::NotifierType::SYSTEM_COMPONENT;
  auto notification = CreateNotification(
      id, u"test_title", u"test_message", ui::ImageModel(), std::u16string(),
      GURL(), notifier_id, message_center::RichNotificationData());
  notification->set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  return id;
}

std::string NotificationCenterTestApi::AddProgressNotification() {
  const auto id = GenerateNotificationId();
  message_center::RichNotificationData optional_fields;
  optional_fields.progress = 50;
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, id, u"test_title",
      u"test_message", /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), GURL(), message_center::NotifierId(),
      optional_fields, new message_center::NotificationDelegate());
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  return id;
}

std::string NotificationCenterTestApi::AddNotificationWithSettingsButton() {
  auto notification = CreateSimpleNotification();
  auto id = notification->id();
  // Setting this to a value other than the default
  // `message_center::SettingsButtonHandler::NONE` makes the settings control
  // button visible.
  notification->set_settings_button_handler(
      message_center::SettingsButtonHandler::DELEGATE);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  return id;
}

std::string NotificationCenterTestApi::AddLowPriorityNotification() {
  auto notification = CreateSimpleNotification();
  auto id = notification->id();
  notification->set_priority(message_center::LOW_PRIORITY);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
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
  return GetTray()->is_active() && GetWidget()->IsVisible();
}

bool NotificationCenterTestApi::IsNotificationCounterShown() {
  return IsNotificationCounterShownOnDisplay(primary_display_id_);
}

bool NotificationCenterTestApi::IsNotificationCounterShownOnDisplay(
    int64_t display_id) {
  return GetNotificationCounterOnDisplay(display_id)->GetVisible();
}

bool NotificationCenterTestApi::IsNotificationIconShown() {
  return IsNotificationIconShownOnDisplay(primary_display_id_);
}

bool NotificationCenterTestApi::IsNotificationIconShownOnDisplay(
    int64_t display_id) {
  auto* notification_center_tray = GetTrayOnDisplay(display_id);
  CHECK(notification_center_tray);
  auto tray_items =
      notification_center_tray->notification_icons_controller_->tray_items();
  CHECK(!tray_items.empty());
  return tray_items.back()->GetVisible();
}

bool NotificationCenterTestApi::IsPopupShown(const std::string& id) {
  return message_center::MessageCenter::Get()->FindPopupNotificationById(id);
}

bool NotificationCenterTestApi::IsTrayShown() {
  return GetTray()->GetVisible();
}

bool NotificationCenterTestApi::IsTrayShownOnDisplay(int64_t display_id) {
  auto* notification_center_tray = GetTrayOnDisplay(display_id);
  CHECK(notification_center_tray);
  return notification_center_tray->GetVisible();
}

bool NotificationCenterTestApi::IsTrayAnimating() {
  return GetTray()->layer()->GetAnimator()->is_animating();
}

bool NotificationCenterTestApi::IsTrayAnimatingOnDisplay(int64_t display_id) {
  auto* notification_center_tray = GetTrayOnDisplay(display_id);
  CHECK(notification_center_tray);
  return notification_center_tray->layer()->GetAnimator()->is_animating();
}

bool NotificationCenterTestApi::IsNotificationCounterAnimating() {
  return IsNotificationCounterAnimatingOnDisplay(primary_display_id_);
}

bool NotificationCenterTestApi::IsNotificationCounterAnimatingOnDisplay(
    int64_t display_id) {
  return GetNotificationCounterOnDisplay(display_id)->IsAnimating();
}

bool NotificationCenterTestApi::IsDoNotDisturbIconShown() {
  return GetTray()
      ->notification_icons_controller_->quiet_mode_view()
      ->GetVisible();
}

NotificationIconTrayItemView*
NotificationCenterTestApi::GetNotificationIconForId(const std::string& id) {
  auto tray_items = GetTray()->notification_icons_controller_->tray_items();
  auto tray_item_iter = base::ranges::find_if(
      tray_items, [&id](NotificationIconTrayItemView* tray_item) {
        return tray_item->GetNotificationId() == id;
      });
  return tray_item_iter == tray_items.end() ? nullptr : *tray_item_iter;
}

NotificationCounterView* NotificationCenterTestApi::GetNotificationCounter() {
  return GetNotificationCounterOnDisplay(primary_display_id_);
}

NotificationCounterView*
NotificationCenterTestApi::GetNotificationCounterOnDisplay(int64_t display_id) {
  auto* notification_center_tray = GetTrayOnDisplay(display_id);
  CHECK(notification_center_tray);
  return notification_center_tray->notification_icons_controller_
      ->notification_counter_view();
}

message_center::MessageView*
NotificationCenterTestApi::GetNotificationViewForId(const std::string& id) {
  return GetNotificationViewForIdOnDisplay(id, primary_display_id_);
}

message_center::MessageView*
NotificationCenterTestApi::GetNotificationViewForIdOnDisplay(
    const std::string& notification_id,
    const int64_t display_id) {
  // Ensure this api is only called when the notification list view exists, i.e.
  // The notification center bubble is open on this display.
  DCHECK(GetNotificationListViewOnDisplay(display_id));

  return GetNotificationListViewOnDisplay(display_id)
      ->GetMessageViewForNotificationId(notification_id);
}

message_center::MessagePopupView* NotificationCenterTestApi::GetPopupViewForId(
    const std::string& id) {
  return GetTray()->popup_collection()->GetPopupViewForNotificationID(id);
}

NotificationCenterTray* NotificationCenterTestApi::GetTray() {
  return Shell::Get()
      ->GetPrimaryRootWindowController()
      ->shelf()
      ->GetStatusAreaWidget()
      ->notification_center_tray();
}

NotificationCenterTray* NotificationCenterTestApi::GetTrayOnDisplay(
    int64_t display_id) {
  auto* root_window_controller =
      Shell::Get()->GetRootWindowControllerWithDisplayId(display_id);
  if (!root_window_controller) {
    return nullptr;
  }
  return root_window_controller->shelf()
      ->status_area_widget()
      ->notification_center_tray();
}

views::Widget* NotificationCenterTestApi::GetWidget() {
  return GetTray()->GetBubbleWidget();
}

NotificationCenterBubble* NotificationCenterTestApi::GetBubble() {
  return GetTray()->bubble_.get();
}

NotificationCenterView*
NotificationCenterTestApi::GetNotificationCenterViewOnDisplay(
    int64_t display_id) {
  if (!Shell::Get()->GetRootWindowControllerWithDisplayId(display_id)) {
    return nullptr;
  }

  return GetTrayOnDisplay(display_id)->bubble_->GetNotificationCenterView();
}

NotificationCenterView* NotificationCenterTestApi::GetNotificationCenterView() {
  return GetNotificationCenterViewOnDisplay(primary_display_id_);
}

NotificationListView* NotificationCenterTestApi::GetNotificationListView() {
  return GetNotificationListViewOnDisplay(primary_display_id_);
}

void NotificationCenterTestApi::CompleteNotificationListAnimation() {
  while (GetNotificationListView()->animation_->is_animating()) {
    GetNotificationListView()->animation_->End();
  }
}

views::View* NotificationCenterTestApi::GetClearAllButton() {
  auto* notification_center_view = GetNotificationCenterView();
  return notification_center_view
             ? notification_center_view->notification_bar_->clear_all_button_
             : nullptr;
}

std::string NotificationCenterTestApi::NotificationIdToParentNotificationId(
    const std::string& id) {
  return id + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                  message_center::MessageCenter::Get()
                      ->FindNotificationById(id)
                      ->notifier_id());
}

views::FocusRing* NotificationCenterTestApi::GetFocusRing() {
  return views::FocusRing::Get(GetTray());
}

void NotificationCenterTestApi::FocusTray() {
  Shell::Get()->focus_cycler()->FocusWidget(GetTray()->GetWidget());
  GetTray()->RequestFocus();
}

std::string NotificationCenterTestApi::GenerateNotificationId() {
  return base::NumberToString(notification_id_++);
}

NotificationListView*
NotificationCenterTestApi::GetNotificationListViewOnDisplay(
    int64_t display_id) {
  DCHECK(message_center::MessageCenter::Get()->IsMessageCenterVisible());

  return GetNotificationCenterViewOnDisplay(display_id)
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
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields) {
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message, icon,
      display_source, url, notifier_id, optional_fields,
      new message_center::NotificationDelegate());
}

std::unique_ptr<message_center::Notification>
NotificationCenterTestApi::CreateSimpleNotification() {
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, GenerateNotificationId(),
      u"test_title", u"test_message", /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::NotificationDelegate());
}

}  // namespace ash
