// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_LIST_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/view.h"

namespace gfx {
class LinearAnimation;
}  // namespace gfx

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

class UnifiedMessageCenterView;
class UnifiedSystemTrayModel;

// Manages list of notifications. The class doesn't know about the ScrollView
// it's enclosed. This class is used only from UnifiedMessageCenterView.
class ASH_EXPORT UnifiedMessageListView
    : public views::View,
      public message_center::MessageCenterObserver,
      public message_center::MessageView::SlideObserver,
      public gfx::AnimationDelegate {
 public:
  // |message_center_view| can be null in unit tests.
  UnifiedMessageListView(UnifiedMessageCenterView* message_center_view,
                         UnifiedSystemTrayModel* model);
  ~UnifiedMessageListView() override;

  // Initializes the view with existing notifications. Should be called right
  // after ctor.
  void Init();

  // Get the height of the notification at the bottom. If no notification is
  // added, it returns 0.
  int GetLastNotificationHeight() const;

  // Count the number of notifications whose bottom position is above
  // |y_offset|. O(n) where n is number of notifications.
  int CountNotificationsAboveY(int y_offset) const;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void PreferredSizeChanged() override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  // message_center::MessageView::SlideObserver:
  void OnSlideChanged(const std::string& notification_id) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  void set_enable_animation(bool enable_animation) {
    enable_animation_ = enable_animation;
  }

 protected:
  // Virtual for testing.
  virtual message_center::MessageView* CreateMessageView(
      const message_center::Notification& notification);

 private:
  friend class UnifiedMessageCenterViewTest;
  friend class UnifiedMessageListViewTest;
  class MessageViewContainer;

  // UnifiedMessageListView always runs single animation at one time. When
  // |state_| is IDLE, animation_->is_animating() is always false and vice
  // versa.
  enum class State {
    // No animation is running.
    IDLE,

    // Sliding out a removed notification. It will transition to MOVE_DOWN.
    SLIDE_OUT,

    // Moving down notifications.
    MOVE_DOWN
  };

  MessageViewContainer* GetContainer(int index);
  const MessageViewContainer* GetContainer(int index) const;

  // Current progress of the animation between 0.0 and 1.0. Returns 1.0 when
  // it's not animating.
  double GetCurrentValue() const;

  // Collapses all the existing notifications. It does not trigger
  // PreferredSizeChanged() (See |ignore_size_change_|).
  void CollapseAllNotifications();

  // Updates the borders of notifications. It adds separators between
  // notifications, and rounds notification corners at the top and the bottom.
  void UpdateBorders();

  // Updates |final_bounds| of all notifications and moves old |final_bounds| to
  // |start_bounds|.
  void UpdateBounds();

  // Resets the animation, and makes all notifications immediately positioned at
  // |final_bounds|.
  void ResetBounds();

  // Deletes all the MessageViewContainer marked as |is_removed|.
  void DeleteRemovedNotifications();

  UnifiedMessageCenterView* const message_center_view_;
  UnifiedSystemTrayModel* const model_;

  // If true, ChildPreferredSizeChanged() will be ignored. This is used in
  // CollapseAllNotifications() to prevent PreferredSizeChanged() triggered
  // multiple times because of sequential SetExpanded() calls.
  bool ignore_size_change_ = false;

  // Manages notification closing animation. UnifiedMessageListView does not use
  // implicit animation.
  const std::unique_ptr<gfx::LinearAnimation> animation_;

  State state_ = State::IDLE;

  // The height the UnifiedMessageListView starts animating from. If not
  // animating, it's ignored.
  int start_height_ = 0;

  // The final height of the UnifiedMessageListView. If not animating, it's same
  // as height().
  int ideal_height_ = 0;

  // If false, disables animation on notification removal.
  bool enable_animation_ = true;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageListView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_LIST_VIEW_H_
