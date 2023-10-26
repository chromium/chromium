// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace views {
class BoxLayout;
class ScrollView;
class ScrollBar;
}  // namespace views

namespace ash {

class NotificationListView;
class StackedNotificationBar;

// Manages scrolling of notification list.
class ASH_EXPORT NotificationCenterView : public views::View,
                                          public views::ViewObserver {
 public:
  METADATA_HEADER(NotificationCenterView);

  NotificationCenterView();

  NotificationCenterView(const NotificationCenterView&) = delete;
  NotificationCenterView& operator=(const NotificationCenterView&) = delete;

  ~NotificationCenterView() override;

  // Initializes the `NotificationListView` with existing notifications.
  // Should be called after ctor.
  void Init();

  // Calls the notification bar `Update` function with the current unpinned,
  // pinned and stacked notification counts. Returns true if the state of the
  // bar has changed.
  bool UpdateNotificationBar();

  // Called from `NotificationListView` when the preferred size is changed.
  void ListPreferredSizeChanged();

  // Called from the `NotificationListView` after a notification is dismissed by
  // the user and the slide animation is finished.
  void OnNotificationSlidOut();

  // Configures `MessageView` to forward scroll events. Called from
  // `NotificationListView`.
  void ConfigureMessageView(message_center::MessageView* message_view);

  // Count number of notifications that are still in the `MessageCenter` that
  // are above visible area. NOTE: views may be in the view hierarchy, but no
  // longer in the message center.
  std::vector<message_center::Notification*> GetStackedNotifications() const;

  // Count the number of notifications that are not visible in the scrollable
  // window, but still in the view hierarchy, with no checks for whether they
  // are in the message center.
  std::vector<std::string> GetNonVisibleNotificationIdsInViewHierarchy() const;

  // Called when user clicks the clear all button.
  void ClearAllNotifications();

  // Returns true if the scroll bar is visible.
  bool IsScrollBarVisible() const;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  NotificationListView* notification_list_view() {
    return notification_list_view_;
  }

 private:
  friend class NotificationCenterTestApi;
  friend class NotificationCenterViewTest;

  // Callback for whenever the scroll view contained in this view receives a
  // scroll event.
  void OnContentsScrolled();

  // Returns the current animation value after tweening.
  double GetAnimationValue() const;

  const raw_ptr<StackedNotificationBar, ExperimentalAsh> notification_bar_;
  raw_ptr<views::ScrollBar, ExperimentalAsh> scroll_bar_;
  const raw_ptr<views::ScrollView, ExperimentalAsh> scroller_;
  const raw_ptr<NotificationListView, ExperimentalAsh> notification_list_view_;

  raw_ptr<views::BoxLayout, ExperimentalAsh> layout_manager_ = nullptr;

  base::CallbackListSubscription on_contents_scrolled_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_VIEW_H_
