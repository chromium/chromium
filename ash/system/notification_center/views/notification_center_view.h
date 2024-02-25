// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_CENTER_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_CENTER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"

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
  METADATA_HEADER(NotificationCenterView, views::View)

 public:
  NotificationCenterView();

  NotificationCenterView(const NotificationCenterView&) = delete;
  NotificationCenterView& operator=(const NotificationCenterView&) = delete;

  ~NotificationCenterView() override;

  // Initializes the `NotificationListView` with existing notifications.
  // Should be called after ctor.
  // Used when `NotificationCenterController` is disabled.
  void Init();
  // Used when `NotificationCenterController` is enabled, but `OngoingProcesses`
  // are disabled.
  void Init(const std::vector<message_center::Notification*>& notifications);
  // Used when `OngoingProcesses` are enabled.
  void Init(
      const std::vector<message_center::Notification*>& unpinned_notifications,
      std::unique_ptr<views::View> pinned_notification_list_view);

  // Inits `scroller_`, adds `notification_list_view_` as its child view and
  // adds `notification_bar_` as a child view of the center view.
  // A `pinned_notification_list_view` is received whenever `OngoingProcesses`
  // are enabled.
  void AddChildViews(
      std::unique_ptr<views::View> pinned_notification_list_view = nullptr);

  // Calls the notification bar `Update` function with the current unpinned,
  // pinned and stacked notification counts. Returns true if the state of the
  // bar has changed.
  bool UpdateNotificationBar();

  // Called from `NotificationListView` when the preferred size is changed.
  void ListPreferredSizeChanged();

  // Called from the `NotificationListView` after a notification is dismissed by
  // the user and the slide animation is finished.
  void OnNotificationSlidOut();

  // Called from `NotificationCenterController` which observes `MessageCenter`.
  void OnNotificationAdded(const std::string& id);
  void OnNotificationRemoved(const std::string& id, bool by_user);
  void OnNotificationUpdated(const std::string& id);

  // Configures `MessageView` to forward scroll events. Called from
  // `NotificationListView`.
  void ConfigureMessageView(message_center::MessageView* message_view);

  // Count number of notifications that are still in the `MessageCenter` that
  // are above visible area. NOTE: views may be in the view hierarchy, but no
  // longer in the message center.
  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
  GetStackedNotifications() const;

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

  const raw_ptr<StackedNotificationBar> notification_bar_;
  raw_ptr<views::ScrollBar> scroll_bar_;
  const raw_ptr<views::ScrollView> scroller_;
  raw_ptr<NotificationListView> notification_list_view_;

  // ViewTracker used to ensure `notification_list_view_` is cleared immediately
  // on deletion.
  views::ViewTracker notification_list_view_tracker_;

  raw_ptr<views::BoxLayout> layout_manager_ = nullptr;

  base::CallbackListSubscription on_contents_scrolled_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_CENTER_VIEW_H_
