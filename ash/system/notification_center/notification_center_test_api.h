// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_

#include <memory>
#include <string>

#include "ash/system/unified/notification_icons_controller.h"
#include "base/strings/string_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

class GURL;

namespace message_center {
class MessagePopupView;
class MessageView;
class Notification;
struct NotifierId;
}  // namespace message_center

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class FocusRing;
class View;
class Widget;
}  // namespace views

namespace ash {

class NotificationCenterBubble;
class NotificationListView;
class NotificationCenterTray;
class NotificationCenterView;
class NotificationCounterView;

// Utility class to facilitate easier testing of the notification center.
class NotificationCenterTestApi {
 public:
  NotificationCenterTestApi();
  NotificationCenterTestApi(const NotificationCenterTestApi&) = delete;
  NotificationCenterTestApi& operator=(const NotificationCenterTestApi&) =
      delete;
  ~NotificationCenterTestApi() = default;

  // Toggles the `NotificationCenterBubble` by
  // simulating a click on the `NotificationCenterTray`
  // on the primary display. Note: This API does not wait for any tray
  // animations to finish before clicking on the tray - that responsibility is
  // left to the caller (this becomes relevant, for instance, when the tray's
  // hide animation is running, as events are disabled for the duration of that
  // animation).
  void ToggleBubble();

  // Toggles the `NotificationCenterBubble` by
  // simulating a click on the `NotificationCenterTray`
  // on the specified display. Note: This API does not wait for any tray
  // animations to finish before clicking on the tray - that responsibility is
  // left to the caller (this becomes relevant, for instance, when the tray's
  // hide animation is running, as events are disabled for the duration of that
  // animation).
  void ToggleBubbleOnDisplay(int64_t dispay_id);

  // Adds a notification with custom parameters and returns the associated id.
  std::string AddCustomNotification(
      const std::u16string& title,
      const std::u16string& message,
      const ui::ImageModel& icon = ui::ImageModel(),
      const std::u16string& display_source = std::u16string(),
      const GURL& url = GURL(),
      const message_center::NotifierId& notifier_id =
          message_center::NotifierId(),
      const message_center::RichNotificationData& optional_fields =
          message_center::RichNotificationData());

  // Adds a notification and returns the associated id.
  std::string AddNotification();

  // Adds a notification with the source url and notifier id corresponding to
  // the provided url as a string. Useful for testing notification grouping.
  std::string AddNotificationWithSourceUrl(const std::string& url);

  // Adds a pinned notification with a source URL, which creates a `NotifierId`
  // object with type `WEB_PAGE`.
  std::string AddPinnedNotificationWithSourceUrl(const std::string& url);

  // Adds a pinned notification and return the associated id.
  std::string AddPinnedNotification();

  // Adds a notification with the system component notifier and system priority
  // level.
  std::string AddSystemNotification();

  // Adds a system notification with a warning level of
  // `SystemNotificationWarningLevel::CRITICAL_WARNING` and returns its id.
  std::string AddCriticalWarningSystemNotification();

  // Adds a progress notification and returns the associated id.
  std::string AddProgressNotification();

  // Adds a notification that should have the settings control button visible
  // and returns its id.
  std::string AddNotificationWithSettingsButton();

  // Adds a notification with priority `LOW_PRIORITY` and returns its id.
  std::string AddLowPriorityNotification();

  // Removes the notification associated with the provided id.
  void RemoveNotification(const std::string& id);

  // Returns the number of notifications in the current notification list.
  size_t GetNotificationCount() const;

  // Returns true if `NotificationCenterBubble` is shown, false otherwise.
  bool IsBubbleShown();

  // Returns true if the notification counter is shown in the primary display's
  // `NotificationCenterTray`.
  bool IsNotificationCounterShown();

  // Returns true if the notification counter is shown in the
  // `NotificationCenterTray` associated with the display having an id of
  // `display_id`. `CHECK()`s that there exists a notification center tray
  // associated with that display.
  bool IsNotificationCounterShownOnDisplay(int64_t display_id);

  // Returns true if a `NotificationIconTrayItemView` is shown in the primary
  // display's `NotificationCenterTray`.
  bool IsNotificationIconShown();

  // Returns true if a `NotificationIconTrayItemView` is shown in the
  // `NotificationCenterTray` associated with the display having an id of
  // `display_id`. `CHECK()`s that there exists a notification center tray
  // associated with that display.
  bool IsNotificationIconShownOnDisplay(int64_t display_id);

  // Returns true if a popup associated with the provided `id` exists, false
  // otherwise.
  bool IsPopupShown(const std::string& id);

  // Returns true if the primary display's `NotificationCenterTray` is showing
  // in the shelf, false otherwise.
  bool IsTrayShown();

  // Returns true if the `NotificationCenterTray` on the display associated with
  // `display_id` is showing in the shelf, false otherwise. `CHECK()`s that
  // there exists a notification center tray associated with that display.
  bool IsTrayShownOnDisplay(int64_t display_id);

  // Returns true if the primary display's `NotificationCenterTray` is
  // animating, false otherwise.
  bool IsTrayAnimating();

  // Returns true if the `NotificationCenterTray` on the display associated with
  // `display_id` is animating, false otherwise. `CHECK()`s that there exists a
  // notification center tray associated with that display.
  bool IsTrayAnimatingOnDisplay(int64_t display_id);

  // Returns true if the primary display's `NotificationCounterView` is
  // animating, false otherwise.
  bool IsNotificationCounterAnimating();

  // Returns true if the `NotificationCounterView` of the
  // `NotificationCenterTray` on the display associated with `display_id` is
  // animating, false otherwise. `CHECK()`s that there exists a notification
  // center tray associated with that display.
  bool IsNotificationCounterAnimatingOnDisplay(int64_t display_id);

  // Returns true if `QuietModeView` is showing in the `NotificationCenterTray`,
  // false otherwise.
  bool IsDoNotDisturbIconShown();

  // Returns the `NotificationIconTrayItemView` associated with the notification
  // that has an id of `id`, or nullptr if none exists.
  NotificationIconTrayItemView* GetNotificationIconForId(const std::string& id);

  // Returns the primary display's `NotificationCounterView`.
  NotificationCounterView* GetNotificationCounter();

  // Returns the `NotificationCounterView` of the `NotificationCenterTray` on
  // the display associated with `display_id`. `CHECK()`s that there exists a
  // notification center tray associated with that display.
  NotificationCounterView* GetNotificationCounterOnDisplay(int64_t display_id);

  // Returns the notification view associated with the provided notification id.
  // Should be only used when the notifications bubble is open.
  message_center::MessageView* GetNotificationViewForId(const std::string& id);

  // Returns the notification view for the provided `notification_id` on the
  // display associated with the provided `display_id`.
  message_center::MessageView* GetNotificationViewForIdOnDisplay(
      const std::string& notification_id,
      const int64_t display_id);

  // Returns the popup view associated with the provided notification id,
  // nullptr otherwise.
  message_center::MessagePopupView* GetPopupViewForId(const std::string& id);

  // Returns the `NotificationCenterTray` for the primary display.
  NotificationCenterTray* GetTray();

  // Returns the `NotificationCenterTray` associated with the display having an
  // id of `display_id`, or nullptr if there is no display with that id.
  NotificationCenterTray* GetTrayOnDisplay(int64_t display_id);

  // Returns the widget that owns the `TrayBubbleView` for the notification
  // center.
  views::Widget* GetWidget();

  // Returns the `NotificationCenterBubble` owned by `NotificationCenterTray`
  // and created when the notification center tray is shown.
  NotificationCenterBubble* GetBubble();

  // Returns the notification center's top level view associated with the
  // provided id, or nullptr if there is no such view associated with the id.
  NotificationCenterView* GetNotificationCenterViewOnDisplay(
      int64_t display_id);

  // Returns the notification center's top level view associated with the
  // primary display.
  NotificationCenterView* GetNotificationCenterView();

  // Returns the parent view for all notification views.
  NotificationListView* GetNotificationListView();

  // Completes all animations that may be running in `NotificationListView`.
  void CompleteNotificationListAnimation();

  // Returns the clear all button in the bottom right corner of the notification
  // center UI.
  views::View* GetClearAllButton();

  // Converts a provided notification id to the corresponding parent
  // notification id by adding the same suffix added by
  // `NotificationGroupingController` to create a parent notification.
  std::string NotificationIdToParentNotificationId(const std::string& id);

  // Returns the notification center tray's focus ring.
  views::FocusRing* GetFocusRing();

  // Focuses the notification center tray.
  void FocusTray();

 private:
  std::string GenerateNotificationId();

  NotificationListView* GetNotificationListViewOnDisplay(int64_t display_id);

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const std::u16string& title,
      const std::u16string& message,
      const ui::ImageModel& icon,
      const std::u16string& display_source,
      const GURL& url,
      const message_center::NotifierId& notifier_id,
      const message_center::RichNotificationData& optional_fields);

  // Creates and returns a `NOTIFICATION_TYPE_SIMPLE` notification with mostly
  // default parameters. The notification's id is obtained via
  // `GenerateNotificationId()`, and the title and description are "test_title"
  // and "test_message", respectively.
  std::unique_ptr<message_center::Notification> CreateSimpleNotification();

  int notification_id_ = 0;

  const int64_t primary_display_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_
