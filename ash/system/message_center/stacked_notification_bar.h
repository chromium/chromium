// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_STACKED_NOTIFICATION_BAR_H_
#define ASH_SYSTEM_MESSAGE_CENTER_STACKED_NOTIFICATION_BAR_H_

#include "ash/ash_export.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// The header shown above the notification list displaying the number of hidden
// notifications. There are currently two UI implementations toggled by the
// NotificationStackedBarRedesign feature flag.
class StackedNotificationBar : public views::View,
                               public message_center::MessageCenterObserver {
 public:
  explicit StackedNotificationBar(
      UnifiedMessageCenterView* message_center_view);
  ~StackedNotificationBar() override;

  // Sets the icons and overflow count for hidden notifications as well as the
  // total/pinned notifications count. Returns true if the state of the bar
  // has changed.
  bool Update(int total_notification_count,
              int pinned_notification_count,
              std::vector<message_center::Notification*> stacked_notifications);

  // Sets the current animation state.
  void SetAnimationState(UnifiedMessageCenterAnimationState animation_state);

  // Set notification bar state to collapsed.
  void SetCollapsed();

  // Set notification bar state to expanded.
  void SetExpanded();

  // Clean up icon view after it's removal animation is complete.
  void OnIconAnimatedOut(views::View* icon);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

 private:
  class StackedNotificationBarIcon;
  friend class UnifiedMessageCenterViewTest;

  // Get the first icon which is not animating out.
  StackedNotificationBarIcon* GetFrontIcon();

  // Search for a icon view in the stacked notification bar based on a provided
  // notification id.
  const StackedNotificationBarIcon* GetIconFromId(const std::string& id) const;

  // Set visibility based on number of stacked notifications or animation state.
  void UpdateVisibility();

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

  UnifiedMessageCenterAnimationState animation_state_ =
      UnifiedMessageCenterAnimationState::IDLE;

  UnifiedMessageCenterView* const message_center_view_;
  views::View* notification_icons_container_;
  views::Label* const count_label_;
  views::Button* const clear_all_button_;
  views::Button* const expand_all_button_;
  DISALLOW_COPY_AND_ASSIGN(StackedNotificationBar);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_STACKED_NOTIFICATION_BAR_H_
