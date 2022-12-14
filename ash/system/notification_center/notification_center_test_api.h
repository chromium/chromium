// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_

#include <memory>
#include <string>

namespace message_center {
class Notification;
}  // namespace message_center

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class NotificationCenterBubble;
class NotificationListView;
class NotificationCenterTray;

// Utility class to facilitate easier testing of the notification center.
class NotificationCenterTestApi {
 public:
  explicit NotificationCenterTestApi(NotificationCenterTray* tray);
  NotificationCenterTestApi(const NotificationCenterTestApi&) = delete;
  NotificationCenterTestApi& operator=(const NotificationCenterTestApi&) =
      delete;
  ~NotificationCenterTestApi() = default;

  // Toggles the `NotificationCenterBubble` by simulating a click on the
  // `NotificationCenterTray` on the primary display.
  void ToggleBubble();

  // Adds a notification and returns the associated id.
  std::string AddNotification();

  // Adds a notification with custom parameters and returns the associated id.
  std::string AddCustomNotification(const std::string& title,
                                    const std::string& message,
                                    const ui::ImageModel& icon);

  // Removes the notification associated with the provided id.
  void RemoveNotification(const std::string& id);

  // Returns the number of notifications in the current notification list.
  size_t GetNotificationCount() const;

  // Returns true if `NotificationCenterBubble` is shown, false otherwise.
  bool IsBubbleShown();

  // Returns true if a popup associated with the provided `id` exists, false
  // otherwise.
  bool IsPopupShown(const std::string& id);

  // Returns true if `NotificationCenterTray` is showing in the shelf, false
  // otherwise.
  bool IsTrayShown();

  // Returns true if `QuietModeView` is showing in the `NotificationCenterTray`,
  // false otherwise.
  bool IsDoNotDisturbIconShown();

  // Returns the notification view associated with the provided notification id.
  // Should be only used when the notifications bubble is open.
  views::View* GetNotificationViewForId(const std::string& id);

  // Returns the popup view associated with the provided notification id,
  // nullptr otherwise.
  views::View* GetPopupViewForId(const std::string& id);

  // Returns the `NotificationCenterTray` in the shelf.
  NotificationCenterTray* GetTray();

  // Returns the widget that owns the `TrayBubbleView` for the notification
  // center.
  views::Widget* GetWidget();

  // Returns the `NotificationCenterBubble` owned by `NotificationCenterTray`
  // and created when the notification center tray is shown.
  NotificationCenterBubble* GetBubble();

  // Returns the top level view for the notification center.
  views::View* GetNotificationCenterView();

  // Returns the clear all button in the bottom right corner of the notification
  // center UI.
  views::View* GetClearAllButton();

 private:
  std::string GenerateNotificationId();

  NotificationListView* GetNotificationListView();

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const std::string& title,
      const std::string& message,
      const ui::ImageModel& icon);

  int notification_id_ = 0;
  NotificationCenterTray* const notification_center_tray_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_
