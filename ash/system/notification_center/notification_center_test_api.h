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

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class NotificationCenterBubble;
class NotificationCenterTray;

// Utility class to facilitate easier testing of the notification center.
class NotificationCenterTestApi {
 public:
  explicit NotificationCenterTestApi(NotificationCenterTray* tray);
  NotificationCenterTestApi(const NotificationCenterTestApi&) = delete;
  NotificationCenterTestApi& operator=(const NotificationCenterTestApi&) =
      delete;
  ~NotificationCenterTestApi() = default;

  // Adds a notification and returns the associated id.
  std::string AddNotification();

  // Removes the notification associated with the provided id.
  void RemoveNotification(const std::string& id);

  // Returns true if `NotificationCenterBubble` is shown, false otherwise.
  bool IsBubbleShown();

  // Returns true if `NotificationCenterTray`` is showing in the shelf, false
  // otherwise.
  bool IsTrayShown();

  // Returns the `NotificationCenterTray` in the shelf.
  NotificationCenterTray* GetTray();

  // Returns the widget that owns the `TrayBubbleView` for the notification
  // center.
  views::Widget* GetWidget();

  // Returns the `NotificationCenterBubble` owned by `NotificationCenterTray`
  // and created when the notification center tray is shown.
  NotificationCenterBubble* GetBubble();

  // Returns the clear all button in the bottom right corner of the notification
  // center UI.
  views::View* GetClearAllButton();

 private:
  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const std::string& title);

  int notification_id_ = 0;
  NotificationCenterTray* const notification_center_tray_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TEST_API_H_