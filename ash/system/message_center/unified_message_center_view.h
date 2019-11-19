// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace gfx {
class LinearAnimation;
}  // namespace gfx

namespace message_center {
class Notification;
}  // namespace message_center

namespace views {
class ScrollView;
}  // namespace views

namespace ash {

class MessageCenterScrollBar;
class StackedNotificationBar;
class UnifiedMessageCenterBubble;
class UnifiedSystemTrayModel;
class UnifiedSystemTrayView;

// Note: This enum represents the current animation state for
// UnifiedMessageCenterView. There is an equivalent animation state emum in
// the child UnifiedMessageListView. The animations for these two views can
// occur simultaneously or independently, so states for both views are tracked
// separately.
enum class UnifiedMessageCenterAnimationState {
  // No animation is running.
  IDLE,

  // Animating hiding the stacking bar. Runs when the user dismisses the
  // second to last notification and during the clear all animation.
  HIDE_STACKING_BAR,

  // Animating collapsing the entire message center. Runs after the user
  // dismisses the last notification and during the clear all animation.
  // TODO(tengs): This animation is not yet implemented.
  COLLAPSE,
};

// Manages scrolling of notification list.
class ASH_EXPORT UnifiedMessageCenterView
    : public views::View,
      public MessageCenterScrollBar::Observer,
      public views::FocusChangeListener,
      public gfx::AnimationDelegate {
 public:
  UnifiedMessageCenterView(UnifiedSystemTrayView* parent,
                           UnifiedSystemTrayModel* model,
                           UnifiedMessageCenterBubble* bubble);
  ~UnifiedMessageCenterView() override;

  // Sets the maximum height that the view can take.
  // TODO(tengs): The layout of this view is heavily dependant on this max
  // height (equal to the height of the entire tray), but we should refactor and
  // consolidate this function with SetAvailableHeight().
  void SetMaxHeight(int max_height);

  // Sets the height available to the message center view. This is the remaining
  // height after counting the system menu, which may be expanded or collapsed.
  void SetAvailableHeight(int available_height);

  // Called from UnifiedMessageListView when the preferred size is changed.
  void ListPreferredSizeChanged();

  // Called from the UnifiedMessageListView after a notification is dismissed by
  // the user and the slide animation is finished.
  void OnNotificationSlidOut();

  // Configures MessageView to forward scroll events. Called from
  // UnifiedMessageListView.
  void ConfigureMessageView(message_center::MessageView* message_view);

  // Count number of notifications that are above visible area.
  std::vector<message_center::Notification*> GetStackedNotifications() const;

  // Relinquish focus and transfer it to the quick settings widget.
  void FocusOut(bool reverse);

  // Set the first child view to be focused when focus is acquired.
  // This is the first visible child unless reverse is true, in which case
  // it is the last visible child.
  void FocusEntered(bool reverse);

  // Expand message center to show all notifications and stacked notification
  // bar if needed.
  void SetExpanded();

  // Collapse the message center to only show the stacked notification bar.
  void SetCollapsed(bool animate);

  // Called when user clicks the clear all button.
  void ClearAllNotifications();

  // Called when user clicks the see all notifications button.
  void ExpandMessageCenter();

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

  // MessageCenterScrollBar::Observer:
  void OnMessageCenterScrolled() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  bool collapsed() { return collapsed_; }

 protected:
  // Virtual for testing.
  virtual void SetNotificationRectBelowScroll(
      const gfx::Rect& rect_below_scroll);

 private:
  friend class UnifiedMessageCenterViewTest;
  friend class UnifiedMessageCenterBubbleTest;

  // Starts the animation to hide the notification stacking bar.
  void StartHideStackingBarAnimation();

  // Starts the animation to collapse the message center.
  void StartCollapseAnimation();

  // Returns the current animation value after tweening.
  double GetAnimationValue() const;

  // Decides whether the message center should be shown or not based on
  // current state.
  void UpdateVisibility();

  // Scroll the notification list to the target position.
  void ScrollToTarget();

  // Notifies rect below scroll to |parent_| so that it can update
  // TopCornerBorder.
  void NotifyRectBelowScroll();

  // Get first and last focusable child views. These functions are used to
  // figure out if we need to focus out or to set the correct focused view
  // when focus is acquired from another widget.
  View* GetFirstFocusableChild();
  View* GetLastFocusableChild();

  UnifiedSystemTrayView* const parent_;
  UnifiedSystemTrayModel* const model_;
  UnifiedMessageCenterBubble* const message_center_bubble_;
  StackedNotificationBar* const notification_bar_;
  MessageCenterScrollBar* const scroll_bar_;
  views::ScrollView* const scroller_;
  UnifiedMessageListView* const message_list_view_;

  // Position from the bottom of scroll contents in dip.
  int last_scroll_position_from_bottom_;

  // The height available to the message center view. This is the remaining
  // height of the system tray excluding the system menu (which can be expanded
  // or collapsed).
  int available_height_ = 0;

  // Current state of the message center view.
  bool collapsed_ = false;

  // Tracks the current animation state.
  UnifiedMessageCenterAnimationState animation_state_ =
      UnifiedMessageCenterAnimationState::IDLE;
  const std::unique_ptr<gfx::LinearAnimation> animation_;

  const std::unique_ptr<views::FocusSearch> focus_search_;

  views::FocusManager* focus_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_VIEW_H_
