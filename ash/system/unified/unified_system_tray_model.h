// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {

// Model class that stores UnifiedSystemTray's UI specific variables. Owned by
// UnifiedSystemTray status area button. Not to be confused with UI agnostic
// SystemTrayModel.
class ASH_EXPORT UnifiedSystemTrayModel {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // |by_user| is true when brightness is changed by user action.
    virtual void OnDisplayBrightnessChanged(bool by_user) {}
    virtual void OnKeyboardBrightnessChanged(bool by_user) {}
  };

  UnifiedSystemTrayModel();
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

  float display_brightness() const { return display_brightness_; }
  float keyboard_brightness() const { return keyboard_brightness_; }

  void set_expanded_on_open(bool expanded_on_open) {
    expanded_on_open_ = expanded_on_open;
  }

 private:
  class DBusObserver;

  void DisplayBrightnessChanged(float brightness, bool by_user);
  void KeyboardBrightnessChanged(float brightness, bool by_user);

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

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_
