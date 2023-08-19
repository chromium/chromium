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
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
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
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

NotificationCenterTestApi::NotificationCenterTestApi(
    NotificationCenterTray* tray)
    : notification_center_tray_(tray),
      primary_display_id_(
          display::Screen::GetScreen()->GetPrimaryDisplay().id()) {}

void NotificationCenterTestApi::ToggleBubble() {
  ToggleBubbleOnDisplay(primary_display_id_);
}

void NotificationCenterTestApi::ToggleBubbleOnDisplay(int64_t display_id) {
  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      Shell::GetRootWindowForDisplayId(display_id));

  auto* status_area_widget =
      Shell::Get()
          ->GetRootWindowControllerWithDisplayId(display_id)
          ->shelf()
          ->status_area_widget();

  gfx::Point click_location =
      features::IsQsRevampEnabled()
          ? status_area_widget->notification_center_tray()
                ->GetBoundsInScreen()
                .CenterPoint()
          : status_area_widget->unified_system_tray()
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

std::string NotificationCenterTestApi::AddNotificationWithSourceUrl(
    const std::string& url) {
  const std::string id = GenerateNotificationId();

  GURL gurl = GURL(url);
  message_center::MessageCenter::Get()->AddNotification(CreateNotification(
      id, u"test_title", u"test_message", ui::ImageModel(),
      base::EmptyString16(), gurl, message_center::NotifierId(gurl),
      message_center::RichNotificationData()));

  return id;
}

std::string NotificationCenterTestApi::AddPinnedNotification() {
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  return AddCustomNotification(
      /*title=*/u"test_title",
      /*message=*/u"test_message", ui::ImageModel(), base::EmptyString16(),
      GURL(), message_center::NotifierId(), optional_fields);
}

std::string NotificationCenterTestApi::AddSystemNotification() {
  message_center::NotifierId notifier_id;
  notifier_id.type = message_center::NotifierType::SYSTEM_COMPONENT;
  message_center::RichNotificationData optional_fields;
  optional_fields.priority =
      message_center::NotificationPriority::SYSTEM_PRIORITY;
  return AddCustomNotification(
      /*title=*/u"test_title",
      /*message=*/u"test_message", ui::ImageModel(), base::EmptyString16(),
      GURL(), notifier_id, optional_fields);
}

std::string NotificationCenterTestApi::AddCriticalWarningSystemNotification() {
  const auto id = GenerateNotificationId();
  message_center::NotifierId notifier_id;
  notifier_id.type = message_center::NotifierType::SYSTEM_COMPONENT;
  auto notification =
      CreateNotification(id, u"test_title", u"test_message", ui::ImageModel(),
                         base::EmptyString16(), GURL(), notifier_id,
                         message_center::RichNotificationData());
  notification->set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
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
  return notification_center_tray_->is_active() && GetWidget()->IsVisible();
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
  return notification_center_tray_->GetVisible();
}

bool NotificationCenterTestApi::IsTrayShownOnDisplay(int64_t display_id) {
  auto* notification_center_tray = GetTrayOnDisplay(display_id);
  CHECK(notification_center_tray);
  return notification_center_tray->GetVisible();
}

bool NotificationCenterTestApi::IsTrayAnimating() {
  return notification_center_tray_->layer()->GetAnimator()->is_animating();
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
  return notification_center_tray_->notification_icons_controller_
      ->quiet_mode_view()
      ->GetVisible();
}

NotificationIconTrayItemView*
NotificationCenterTestApi::GetNotificationIconForId(const std::string& id) {
  auto tray_items =
      notification_center_tray_->notification_icons_controller_->tray_items();
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
  return features::IsQsRevampEnabled() ? notification_center_tray_ : nullptr;
}

NotificationCenterTray* NotificationCenterTestApi::GetTrayOnDisplay(
    int64_t display_id) {
  auto* root_window_controller =
      Shell::Get()->GetRootWindowControllerWithDisplayId(display_id);
  if (!root_window_controller || !features::IsQsRevampEnabled()) {
    return nullptr;
  }
  return root_window_controller->shelf()
      ->status_area_widget()
      ->notification_center_tray();
}

views::Widget* NotificationCenterTestApi::GetWidget() {
  return notification_center_tray_->GetBubbleWidget();
}

NotificationCenterBubble* NotificationCenterTestApi::GetBubble() {
  return notification_center_tray_->bubble_.get();
}

NotificationCenterView*
NotificationCenterTestApi::GetNotificationCenterViewOnDisplay(
    int64_t display_id) {
  if (!Shell::Get()->GetRootWindowControllerWithDisplayId(display_id)) {
    return nullptr;
  }
  auto* status_area_widget =
      Shell::Get()
          ->GetRootWindowControllerWithDisplayId(display_id)
          ->shelf()
          ->status_area_widget();
  return features::IsQsRevampEnabled()
             ? status_area_widget->notification_center_tray()
                   ->bubble_->notification_center_view_.get()
             : status_area_widget->unified_system_tray()
                   ->message_center_bubble()
                   ->notification_center_view();
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
  return notification_center_tray_->bubble_->notification_center_view_
      ->notification_bar_->clear_all_button_;
}

std::string NotificationCenterTestApi::NotificationIdToParentNotificationId(
    const std::string& id) {
  return id + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                  message_center::MessageCenter::Get()
                      ->FindNotificationById(id)
                      ->notifier_id());
}

views::FocusRing* NotificationCenterTestApi::GetFocusRing() const {
  return views::FocusRing::Get(notification_center_tray_);
}

void NotificationCenterTestApi::FocusTray() {
  Shell::Get()->focus_cycler()->FocusWidget(
      notification_center_tray_->GetWidget());
  notification_center_tray_->RequestFocus();
}

std::string NotificationCenterTestApi::GenerateNotificationId() {
  return base::NumberToString(notification_id_++);
}

NotificationListView*
NotificationCenterTestApi::GetNotificationListViewOnDisplay(
    int64_t display_id) {
  DCHECK(message_center::MessageCenter::Get()->IsMessageCenterVisible());

  auto* status_area_widget =
      Shell::Get()
          ->GetRootWindowControllerWithDisplayId(display_id)
          ->shelf()
          ->GetStatusAreaWidget();

  if (features::IsQsRevampEnabled()) {
    return status_area_widget->notification_center_tray()
        ->bubble_->notification_center_view_->notification_list_view();
  }

  return status_area_widget->unified_system_tray()
      ->message_center_bubble()
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
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields) {
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message, icon,
      display_source, url, notifier_id, optional_fields,
      new message_center::NotificationDelegate());
}

}  // namespace ash
