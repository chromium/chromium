// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace display {
class Display;
}  // namespace display

namespace ash {

class Shelf;

// Model class that stores UnifiedSystemTray's UI specific variables. Owned by
// UnifiedSystemTray status area button. Not to be confused with UI agnostic
// SystemTrayModel.
class ASH_EXPORT UnifiedSystemTrayModel
    : public base::RefCounted<UnifiedSystemTrayModel> {
 public:
  enum class StateOnOpen {
    // The user has not made any changes to the quick settings state.
    UNSET,
    // Quick settings has been explicitly set to collapsed by the user.
    COLLAPSED,
    // Quick settings has been explicitly set to expanded by the user.
    EXPANDED
  };

  enum class NotificationTargetMode {
    // Notification list scrolls to the last notification.
    LAST_NOTIFICATION,
    // Notification list scrolls to the last scroll position.
    LAST_POSITION,
    // Notification list scrolls to the specified notification defined by
    // |SetTargetNotification(notification_id)|.
    NOTIFICATION_ID,
  };

  // Enumeration of possible sizes of the system tray button. Larger screen will
  // have larger tray button with additional information.
  enum class SystemTrayButtonSize {
    // Display wifi, battery, notification counter icons and time.
    kSmall = 0,
    // Display those in small unified system tray, plus important notification
    // icons.
    kMedium = 1,
    // Display those in medium unified system tray, plus the current date.
    kLarge = 2,
    kMaxValue = kLarge,
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // |by_user| is true when brightness is changed by user action.
    virtual void OnDisplayBrightnessChanged(bool by_user) {}
    virtual void OnKeyboardBrightnessChanged(
        power_manager::BacklightBrightnessChange_Cause cause) {}
    virtual void OnSystemTrayButtonSizeChanged(
        SystemTrayButtonSize system_tray_size) {}
  };

  explicit UnifiedSystemTrayModel(Shelf* shelf);

  UnifiedSystemTrayModel(const UnifiedSystemTrayModel&) = delete;
  UnifiedSystemTrayModel& operator=(const UnifiedSystemTrayModel&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the tray should be expanded when initially opened.
  bool IsExpandedOnOpen() const;

  // Returns true if the user explicitly set the tray to its
  // expanded state.
  bool IsExplicitlyExpanded() const;

  // Returns empty if it's not manually expanded/collapsed. Otherwise, the value
  // is true if the notification is manually expanded, and false if it's
  // manually collapsed.
  absl::optional<bool> GetNotificationExpanded(
      const std::string& notification_id) const;

  // Sets a notification of |notification_id| is manually |expanded|.
  void SetNotificationExpanded(const std::string& notification_id,
                               bool expanded);

  // Removes the state of the notification of |notification_id|.
  void RemoveNotificationExpanded(const std::string& notification_id);

  // Clears all changes by SetNotificationExpanded().
  void ClearNotificationChanges();

  // Set the notification id of the target. This sets target mode as
  // NOTIFICATION_ID.
  void SetTargetNotification(const std::string& notification_id);

  // Get the size of the system tray depends on the size of the display screen.
  SystemTrayButtonSize GetSystemTrayButtonSize() const;

  float display_brightness() const { return display_brightness_; }
  float keyboard_brightness() const { return keyboard_brightness_; }

  void set_expanded_on_open(StateOnOpen expanded_on_open) {
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
  friend class UnifiedSystemTrayControllerTest;
  // Required for private destructor to be called from RefCounted<>.
  friend class base::RefCounted<UnifiedSystemTrayModel>;

  class DBusObserver;

  // Keeps track all the sources that can change the size of system tray button.
  class SizeObserver;

  // Private destructor to prevent subverting reference counting.
  // TODO(crbug/1269517): The use of this class should be refactored so that
  // reference counting is not required. Likely, Message Center and Quick
  // Settings will need to be combined.
  ~UnifiedSystemTrayModel();

  void DisplayBrightnessChanged(float brightness, bool by_user);
  void KeyboardBrightnessChanged(
      float brightness,
      power_manager::BacklightBrightnessChange_Cause cause);
  void SystemTrayButtonSizeChanged(SystemTrayButtonSize system_tray_size);

  // Get the display that owns the tray.
  const display::Display GetDisplay() const;

  // Target mode which is used to decide the scroll position of the message
  // center on opening. See the comment in |NotificationTargetMode|.
  NotificationTargetMode notification_target_mode_ =
      NotificationTargetMode::LAST_NOTIFICATION;
  // Set the notification id of the target. This id is used if the target mode
  // is NOTIFICATION_ID.
  std::string notification_target_id_;

  // If UnifiedSystemTray bubble is expanded on its open. It's expanded by
  // default, and if a user collapses manually, it remembers previous state.
  StateOnOpen expanded_on_open_ = StateOnOpen::UNSET;

  // The last value of the display brightness slider. Between 0.0 and 1.0.
  float display_brightness_ = 1.f;

  // The last value of the keyboard brightness slider. Between 0.0 and 1.0.
  float keyboard_brightness_ = 1.f;

  // Stores Manual changes to notification expanded / collapsed state in order
  // to restore on reopen.
  // <notification ID, if notification is manually expanded>
  std::map<std::string, bool> notification_changes_;

  Shelf* const shelf_;

  std::unique_ptr<DBusObserver> dbus_observer_;

  std::unique_ptr<SizeObserver> size_observer_;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<PaginationModel> pagination_model_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_MODEL_H_
