// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_

#include <cstdint>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/display_observer.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class NotificationCenterTray;
class NotificationCounterView;
class NotificationIconsController;
class QuietModeView;
class SeparatorTrayItemView;
class TrayContainer;
class TrayItemView;
class UnifiedSystemTray;

// Tray item view for notification icon shown in the tray.
class ASH_EXPORT NotificationIconTrayItemView : public TrayItemView {
 public:
  NotificationIconTrayItemView(Shelf* shelf,
                               NotificationIconsController* controller_);
  ~NotificationIconTrayItemView() override;
  NotificationIconTrayItemView(const NotificationIconTrayItemView&) = delete;
  NotificationIconTrayItemView& operator=(const NotificationIconTrayItemView&) =
      delete;

  // Set the image and tooltip for the view according to the notification.
  void SetNotification(message_center::Notification* notification);

  // Calls `Reset()` immediately or does nothing according to the following
  // flow:
  //   1. If the QS revamp is not enabled then `Reset()` is called immediately.
  //   2. Otherwise, if this tray item is currently running its hide animation,
  //      or if the `NotificationCenterTray` it belongs to is running its hide
  //      animation, then this does nothing (because `Reset()` will be called
  //      at the end of the relevant animation).
  //   3. Otherwise, `Reset()` is still called immediately.
  void MaybeReset();

  // Returns a string describing the current state for accessibility.
  const std::u16string& GetAccessibleNameString() const;

  const std::string& GetNotificationId() const;

  // TrayItemView:
  void HandleLocaleChange() override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;
  void UpdateLabelOrImageViewColor(bool active) override;
  void ImmediatelyUpdateVisibility() override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  void UpdateImageViewColor();

  // Resets notification pointer, id, image and tooltip text.
  void Reset();

  // Store the id to make sure we still have it when notification is removed and
  // goes out of scope.
  std::string notification_id_;

  // Stores the notification to update its icon color based on the active state
  // of this tray.
  std::unique_ptr<message_center::Notification> notification_;

  const raw_ptr<NotificationIconsController,
                DanglingUntriaged | ExperimentalAsh>
      controller_;
};

// Controller for notification icons in `UnifiedSystemTray` button. If the
// QsRevamp feature is enabled, this is used in `NotificationCenterTray`
// instead, and has the added responsibility of letting the
// `NotificationCenterTray` know when it may need to update its visibility. The
// icons will be displayed in medium or large screen size and only for important
// notifications.
class ASH_EXPORT NotificationIconsController
    : public UnifiedSystemTrayModel::Observer,
      public display::DisplayObserver,
      public message_center::MessageCenterObserver,
      public SessionObserver {
 public:
  explicit NotificationIconsController(
      Shelf* shelf,
      UnifiedSystemTrayModel* model = nullptr,
      NotificationCenterTray* notification_center_tray = nullptr);
  ~NotificationIconsController() override;
  NotificationIconsController(const NotificationIconsController&) = delete;
  NotificationIconsController& operator=(const NotificationIconsController&) =
      delete;

  // Initialize the view by adding items to the container of the tray.
  void AddNotificationTrayItems(TrayContainer* tray_container);

  // Returns true if any item in `tray_items_` is containing a notification.
  bool TrayItemHasNotification() const;

  // Returns the number of notification icons showing in |tray_items_|.
  size_t TrayNotificationIconsCount() const;

  // Returns a string describing the current state for accessibility.
  std::u16string GetAccessibleNameString() const;

  // Iterate through the notifications in message center and update the icons
  // shown accordingly.
  void UpdateNotificationIcons();

  // Update notification indicators, including counters and quiet mode view.
  void UpdateNotificationIndicators();

  // UnifiedSystemTrayModel::Observer:
  void OnSystemTrayButtonSizeChanged(
      UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  std::vector<NotificationIconTrayItemView*> tray_items() {
    return tray_items_;
  }

  NotificationCounterView* notification_counter_view() {
    return notification_counter_view_;
  }

  QuietModeView* quiet_mode_view() { return quiet_mode_view_; }

  bool icons_view_visible() const { return icons_view_visible_; }

 private:
  friend class NotificationIconsControllerTest;

  // If the notification with given id is currently shown in tray, returns the
  // pointer to that tray item. Otherwise, returns a null pointer.
  NotificationIconTrayItemView* GetNotificationIconShownInTray(
      const std::string& id);

  // Updates `icons_view_visible_` according to the size of the display
  // associated with `shelf_`.
  void UpdateIconsViewVisibleForDisplaySize();

  // Contains notification icon tray items that are added to tray container. All
  // items are owned by views hierarchy.
  std::vector<NotificationIconTrayItemView*> tray_items_;

  // Points to the first item that is available to use among the notification
  // icons tray item. All the items in previous index are used and visible.
  size_t first_unused_item_index_ = 0;

  // Indicates if the notification icons view is set to be shown. Currently, we
  // show the icon view in medium or large screen size.
  bool icons_view_visible_ = false;

  // Owned by `RootWindowController`
  const raw_ptr<Shelf, ExperimentalAsh> shelf_;

  // Owned by `UnifiedSystemTray`
  const raw_ptr<UnifiedSystemTrayModel, ExperimentalAsh> system_tray_model_;

  // `NotificationCenterTray` owns this `NotificationIconsController` when the
  // QS revamp is enabled. `NotificationCenterTray` itself is owned by the views
  // hierarchy. Note that this will always be null if the QS revamp is not
  // enabled.
  raw_ptr<NotificationCenterTray, ExperimentalAsh> notification_center_tray_ =
      nullptr;

  raw_ptr<NotificationCounterView, ExperimentalAsh> notification_counter_view_ =
      nullptr;
  raw_ptr<QuietModeView, ExperimentalAsh> quiet_mode_view_ = nullptr;
  raw_ptr<SeparatorTrayItemView, ExperimentalAsh> separator_ = nullptr;

  base::ScopedObservation<UnifiedSystemTrayModel,
                          UnifiedSystemTrayModel::Observer>
      system_tray_model_observation_{this};

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
