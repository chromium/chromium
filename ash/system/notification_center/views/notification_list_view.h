// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_LIST_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace gfx {
class LinearAnimation;
}  // namespace gfx

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

class NotificationCenterView;
class MessageViewContainer;

// Manages list of notifications. The class doesn't know about the ScrollView
// it's enclosed. This class is used only from NotificationCenterView.
class ASH_EXPORT NotificationListView
    : public views::View,
      public message_center::MessageCenterObserver,
      public message_center::NotificationViewController,
      public message_center::MessageView::Observer,
      public views::AnimationDelegateViews {
  METADATA_HEADER(NotificationListView, views::View)

 public:
  // |message_center_view| can be null in unit tests.
  explicit NotificationListView(NotificationCenterView* message_center_view);
  NotificationListView(const NotificationListView& other) = delete;
  NotificationListView& operator=(const NotificationListView& other) = delete;
  ~NotificationListView() override;

  // Initializes the view with existing notifications. Should be called right
  // after ctor.
  // Used when `NotificationCenterController` is disabled.
  void Init();
  // Used when `NotificationCenterController` is enabled.
  void Init(const std::vector<message_center::Notification*>& notifications);

  // Starts Clear All animation and removes all notifications. Notifications are
  // removed from MessageCenter at the beginning of the animation.
  void ClearAllWithAnimation();

  // Return the bounds of the specified notification view. If the given id is
  // invalid, return an empty rect.
  gfx::Rect GetNotificationBounds(const std::string& id) const;

  // Return the bounds of the last notification view. If there is no view,
  // return an empty rect.
  gfx::Rect GetLastNotificationBounds() const;

  // Return the bounds of the first notification whose bottom is below
  // |y_offset|.
  gfx::Rect GetNotificationBoundsBelowY(int y_offset) const;

  // Returns all notifications in the view hierarchy that are also in the
  // MessageCenter.
  std::vector<message_center::Notification*> GetAllNotifications() const;

  // Returns all notification ids in the view hierarchy regardless of whether
  // they are in also in the MessageCenter.
  std::vector<std::string> GetAllNotificationIds() const;

  // Returns the notifications in the view hierarchy that are also in the
  // MessageCenter, whose bottom position is above |y_offset|. O(n) where n is
  // number of notifications.
  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
  GetNotificationsAboveY(int y_offset) const;

  // Returns the notifications in the view hierarchy that are also in the
  // MessageCenter, whose bottom position is below |y_offset|. O(n) where n is
  // number of notifications.
  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
  GetNotificationsBelowY(int y_offset) const;

  // Same as GetNotificationsAboveY, but returns notifications that are not in
  // the MessageCenter. This is useful for the clear all animation which first
  // removes all notifications before asking for stacked notifications.
  std::vector<std::string> GetNotificationIdsAboveY(int y_offset) const;

  // Returns notifications that are in the view hierarchy below `y_offset`
  // without checking whether they are in the MessageCenter. This is useful for
  // the clear all animation which first removes all notifications before asking
  // for stacked notifications.
  std::vector<std::string> GetNotificationIdsBelowY(int y_offset) const;

  // Returns the total number of notifications in the list.
  int GetTotalNotificationCount() const;

  // Returns the total number of pinned notifications in the list.
  int GetTotalPinnedNotificationCount() const;

  // Returns true if `animation_` is currently in progress.
  bool IsAnimating() const;

  // Current progress of the animation between 0.0 and 1.0. Returns 1.0 when
  // it's not animating.
  double GetCurrentAnimationValue() const;

  // Returns whether `message_view_container` is being animated for expand or
  // collapse.
  bool IsAnimatingExpandOrCollapseContainer(const views::View* view) const;

  // Called when a notification is slid out so we can run the MOVE_DOWN
  // animation.
  void OnNotificationSlidOut();

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void PreferredSizeChanged() override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // message_center::NotificationViewController:
  void AnimateResize() override;
  message_center::MessageView* GetMessageViewForNotificationId(
      const std::string& id) override;
  void ConvertNotificationViewToGroupedNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) override;
  void ConvertGroupedNotificationViewToNotificationView(
      const std::string& grouped_notification_id,
      const std::string& new_single_notification_id) override;
  void OnChildNotificationViewUpdated(
      const std::string& parent_notification_id,
      const std::string& child_notification_id) override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  // message_center::MessageView::Observer:
  void OnSlideStarted(const std::string& notification_id) override;
  void OnCloseButtonPressed(const std::string& notification_id) override;
  void OnSettingsButtonPressed(const std::string& notification_id) override;
  void OnSnoozeButtonPressed(const std::string& notification_id) override;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  bool is_deleting_removed_notifications() const {
    return is_deleting_removed_notifications_;
  }

 protected:
  // Virtual for testing.
  virtual std::unique_ptr<message_center::MessageView> CreateMessageView(
      const message_center::Notification& notification);

  void ConfigureMessageView(message_center::MessageView* message_view);

  // Virtual for testing.
  virtual std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
  GetStackedNotifications() const;

  // Virtual for testing.
  virtual std::vector<std::string> GetNonVisibleNotificationIdsInViewHierarchy()
      const;

 private:
  friend class NotificationCenterTestApi;
  friend class NotificationCenterViewTest;
  friend class NotificationListViewTest;
  friend class UnifiedMessageCenterBubbleTest;
  class Background;

  // NotificationListView always runs a single animation at one time. When
  // `state_` is IDLE, `animation_->is_animating()` is always false and vice
  // versa.
  enum class State {
    // No animation is running.
    IDLE,

    // Moving down notifications.
    MOVE_DOWN,

    // Part 1 of Clear All animation. Removing all hidden notifications above
    // the visible area.
    CLEAR_ALL_STACKED,

    // Part 2 of Clear All animation. Removing all visible notifications.
    CLEAR_ALL_VISIBLE,

    // Animating an increase or decrease in height of a notification. Only one
    // may animate at a time.
    EXPAND_OR_COLLAPSE
  };

  // Syntactic sugar to downcast.
  static const MessageViewContainer* AsMVC(const views::View* v);
  static MessageViewContainer* AsMVC(views::View* v);

  // Returns the notification with the provided |id|.
  const MessageViewContainer* GetNotificationById(const std::string& id) const;
  MessageViewContainer* GetNotificationById(const std::string& id) {
    return const_cast<MessageViewContainer*>(
        static_cast<const NotificationListView*>(this)->GetNotificationById(
            id));
  }

  // Returns the first removable notification from the top.
  MessageViewContainer* GetNextRemovableNotification();

  // Collapses all the existing notifications. It does not trigger
  // PreferredSizeChanged() (See |ignore_size_change_|).
  void CollapseAllNotifications();

  // Updates the borders of notifications. It adds separators between
  // notifications, and rounds notification corners at the top and the bottom.
  // `force_update` indicates if we should update borders on all notifications
  // regardless of their previous state.
  void UpdateBorders(bool force_update);

  // Updates |final_bounds| of all notifications and moves old |final_bounds| to
  // |start_bounds|.
  void UpdateBounds();

  // Resets the animation, and makes all notifications immediately positioned at
  // |final_bounds|.
  void ResetBounds();

  // Interrupts clear all animation and deletes all the remaining notifications.
  // ResetBounds() should be called after that.
  void InterruptClearAll();

  // Deletes all the MessageViewContainer marked as |is_removed|.
  void DeleteRemovedNotifications();

  // Starts the animation for current |state_|.
  void StartAnimation();

  // Updates the state between each Clear All animation phase.
  void UpdateClearAllAnimation();

  const raw_ptr<NotificationCenterView> message_center_view_;

  // Non-null during State::EXPAND_OR_COLLAPSE. Keeps track of the
  // MessageViewContainer that is animating.
  raw_ptr<MessageViewContainer> expand_or_collapsing_container_ = nullptr;

  // If true, ChildPreferredSizeChanged() will be ignored. Used to prevent
  // PreferredSizeChanged() triggered by system SetExpanded() calls.
  bool ignore_size_change_ = false;

  // If true, OnNotificationRemoved() will be ignored. Used in
  // ClearAllWithAnimation().
  bool ignore_notification_remove_ = false;

  // Manages notification closing animation. `NotificationListView` does not use
  // implicit animation.
  const std::unique_ptr<gfx::LinearAnimation> animation_;

  // Measure animation smoothness metrics for `animation_`.
  std::optional<ui::ThroughputTracker> throughput_tracker_;

  State state_ = State::IDLE;

  // The height the `NotificationListView` starts animating from. If not
  // animating, it's ignored.
  int start_height_ = 0;

  // The final height of `the NotificationListView`. If not animating, it's same
  // as height().
  int target_height_ = 0;

  // True if the `NotificationListView` is currently deleting notifications
  // marked for removal. This check is needed to prevent re-entrancing issues
  // (e.g. crbug.com/933327) caused by the View destructor.
  bool is_deleting_removed_notifications_ = false;

  const int message_view_width_;

  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      message_center_observation_{this};

  base::ScopedMultiSourceObservation<message_center::MessageView,
                                     message_center::MessageView::Observer>
      message_view_multi_source_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_LIST_VIEW_H_
