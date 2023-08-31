// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_STACKED_NOTIFICATION_BAR_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_STACKED_NOTIFICATION_BAR_H_

#include "ash/system/notification_center/notification_center_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace views {
class BoxLayout;
class Label;
}  // namespace views

namespace ash {

// The header shown above the notification list displaying the number of hidden
// notifications. Has a dynamic list of icons which hide/show as notifications
// are scrolled.
class StackedNotificationBar : public views::View,
                               public message_center::MessageCenterObserver {
 public:
  explicit StackedNotificationBar(
      NotificationCenterView* notification_center_view);

  StackedNotificationBar(const StackedNotificationBar&) = delete;
  StackedNotificationBar& operator=(const StackedNotificationBar&) = delete;

  ~StackedNotificationBar() override;

  // Sets the icons and overflow count for hidden notifications as well as the
  // total/pinned notifications count. Returns true if the state of the bar
  // has changed.
  bool Update(int total_notification_count,
              int pinned_notification_count,
              std::vector<message_center::Notification*> stacked_notifications);

  // Sets the current animation state.
  void SetAnimationState(NotificationCenterAnimationState animation_state);

  // Set notification bar state to collapsed.
  void SetCollapsed();

  // Set notification bar state to expanded.
  void SetExpanded();

  // views::View:
  const char* GetClassName() const override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

 private:
  class StackedNotificationBarIcon;
  friend class NotificationCenterTestApi;
  friend class NotificationCenterViewTest;

  // Clean up icon view after it's removal animation is complete, adds an icon
  // for `notification` if needed. Called from a callback registered in
  // `ShiftIconsLeft()`.
  void OnIconAnimatedOut(std::string notification_id, views::View* icon);

  // Get the first icon which is `animating_out`.
  StackedNotificationBarIcon* GetFrontIcon(bool animating_out);

  // Search for a icon view in the stacked notification bar based on a provided
  // notification id.
  const StackedNotificationBarIcon* GetIconFromId(const std::string& id) const;

  // Add a stacked notification icon to the front or back of the row.
  void AddNotificationIcon(message_center::Notification* notification,
                           bool at_front);

  // Move all icons left when notifications are scrolled up.
  void ShiftIconsLeft(
      std::vector<message_center::Notification*> stacked_notifications);

  // Move icons right to make space for additional icons when notifications are
  // scrolled down.
  void ShiftIconsRight(
      std::vector<message_center::Notification*> stacked_notifications);

  // Update state for stacked notification icons and move them as necessary.
  void UpdateStackedNotifications(
      std::vector<message_center::Notification*> stacked_notifications);

  int total_notification_count_ = 0;
  int pinned_notification_count_ = 0;
  int stacked_notification_count_ = 0;

  NotificationCenterAnimationState animation_state_ =
      NotificationCenterAnimationState::kIdle;

  const raw_ptr<NotificationCenterView, ExperimentalAsh>
      notification_center_view_;
  raw_ptr<views::View, ExperimentalAsh> notification_icons_container_;
  const raw_ptr<views::Label, ExperimentalAsh> count_label_;
  const raw_ptr<views::View, ExperimentalAsh> spacer_;
  const raw_ptr<views::Button, ExperimentalAsh> clear_all_button_;
  const raw_ptr<views::Button, ExperimentalAsh> expand_all_button_;
  const raw_ptr<views::BoxLayout, ExperimentalAsh> layout_manager_;

  base::WeakPtrFactory<StackedNotificationBar> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_STACKED_NOTIFICATION_BAR_H_
