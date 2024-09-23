// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_BUBBLE_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_BUBBLE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/screen_layout_observer.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class MessageViewContainer;
class NotificationCenterController;
class NotificationCenterTray;
class NotificationCenterView;
class TrayBubbleView;
class TrayBubbleWrapper;

// Manages the bubble that contains NotificationCenterView.
// Shows the bubble on `ShowBubble()`, and closes the bubble on the destructor.
class ASH_EXPORT NotificationCenterBubble : public ScreenLayoutObserver {
 public:
  explicit NotificationCenterBubble(
      NotificationCenterTray* notification_center_tray);

  NotificationCenterBubble(const NotificationCenterBubble&) = delete;
  NotificationCenterBubble& operator=(const NotificationCenterBubble&) = delete;

  ~NotificationCenterBubble() override;

  // Initializes the `NotificationCenterView` which results in notifications
  // being added to the view. This is necessary outside of the constructor
  // because the `NotificationGroupingController` expects
  // `NotificationCenterBubble` to be fully constructed when notifications are
  // added to it.
  void ShowBubble();

  TrayBubbleView* GetBubbleView();
  views::Widget* GetBubbleWidget();

  // Based on the `NotificationCenterController` feature:
  // Returns `notification_center_view_` when the feature is disabled.
  // Returns the view cached in `notification_center_controller_` when enabled.
  NotificationCenterView* GetNotificationCenterView();

  // Forwards call to `NotificationCenterController`. This currently only
  // searches for ongoing process notifications.
  // TODO(b/322835713): Have the controller manage other notification views.
  const MessageViewContainer* GetOngoingProcessMessageViewContainerById(
      const std::string& id);

 private:
  friend class NotificationCenterTestApi;

  // Update the max height and anchor rect for the bubble.
  void UpdateBubbleBounds();

  // ScreenLayoutObserver:
  void OnDidApplyDisplayChanges() override;

  // The owner of this class.
  const raw_ptr<NotificationCenterTray> notification_center_tray_;

  // The main view responsible for showing all notification content in this
  // bubble. Owned by `TrayBubbleView`.
  // Used when `NotificationCenterController` is disabled.
  raw_ptr<NotificationCenterView> notification_center_view_ = nullptr;

  // The controller responsible for managing the NotificationCenterView and its
  // children including the `StackedNotificationBar` and `NotificationListView`.
  // Used when `NotificationCenterController` is enabled.
  std::unique_ptr<NotificationCenterController> notification_center_controller_;

  std::unique_ptr<TrayBubbleView> bubble_view_;
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_BUBBLE_H_
