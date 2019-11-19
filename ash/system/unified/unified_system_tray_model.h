// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

// Model class that stores UnifiedSystemTray's UI specific variables. Owned by
// UnifiedSystemTray status area button. Not to be confused with UI agnostic
// SystemTrayModel.
class ASH_EXPORT UnifiedSystemTrayModel {
 public:
  enum class NotificationTargetMode {
    // Notification list scrolls to the last notification.
    LAST_NOTIFICATION,
    // Notification list scrolls to the last scroll position.
    LAST_POSITION,
    // Notification list scrolls to the specified notification defined by
    // |SetTargetNotification(notification_id)|.
    NOTIFICATION_ID,
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // |by_user| is true when brightness is changed by user action.
    virtual void OnDisplayBrightnessChanged(bool by_user) {}
    virtual void OnKeyboardBrightnessChanged(bool by_user) {}
  };

  explicit UnifiedSystemTrayModel(views::View* owner_view);
  ~UnifiedSystemTrayModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsExpandedOnOpen() const;

  // Returns empty if it's not manually expanded/collapsed. Otherwise, the value
  // is true if the notification is manually expanded, and false if it's
  // manually collapsed.
  base::Optional<bool> GetNotificationExpanded(
      const std::string& notification_id) const;

  // Sets a notification of |notification_id| is manually |expanded|.
  void SetNotificationExpanded(const std::string& notification_id,
                               bool expanded);

  // Removes the state of the notification of |notification_id|.
  void RemoveNotificationExpanded(const std::string& notification_id);

  // Clears all changes by SetNotificatinExpanded().
  void ClearNotificationChanges();

  // Set the notification id of the target. This sets target mode as
  // NOTIFICATION_ID.
  void SetTargetNotification(const std::string& notification_id);

  float display_brightness() const { return display_brightness_; }
  float keyboard_brightness() const { return keyboard_brightness_; }

  void set_expanded_on_open(bool expanded_on_open) {
    expanded_on_open_ = expanded_on_open;
  }

  void set_notification_target_mode(NotificationTargetMode mode) {
    notification_target_mode_ = mode;
  }

  NotificationTargetMode notification_target_mode() const {
    return notification_target_mode_;
  }

  const std::string& notification_target_id() const {
    return notification_target_id_;
  }

  PaginationModel* pagination_model() { return pagination_model_.get(); }

 private:
  class DBusObserver;

  void DisplayBrightnessChanged(float brightness, bool by_user);
  void KeyboardBrightnessChanged(float brightness, bool by_user);

  // Target mode which is used to decide the scroll position of the message
  // center on opening. See the comment in |NotificationTargetMode|.
  NotificationTargetMode notification_target_mode_ =
      NotificationTargetMode::LAST_NOTIFICATION;
  // Set the notification id of the target. This id is used if the target mode
  // is NOTIFICATION_ID.
  std::string notification_target_id_;

  // If UnifiedSystemTray bubble is expanded on its open. It's expanded by
  // default, and if a user collapses manually, it remembers previous state.
  bool expanded_on_open_ = true;

  // The last value of the display brightness slider. Between 0.0 and 1.0.
  float display_brightness_ = 1.f;

  // The last value of the keyboard brightness slider. Between 0.0 and 1.0.
  float keyboard_brightness_ = 1.f;

  // Stores Manual changes to notification expanded / collapsed state in order
  // to restore on reopen.
  // <notification ID, if notification is manually expanded>
  std::map<std::string, bool> notification_changes_;

  std::unique_ptr<DBusObserver> dbus_observer_;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<PaginationModel> pagination_model_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_
