// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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
class BoxLayout;
class ScrollView;
class ScrollBar;
}  // namespace views

namespace ash {

class StackedNotificationBar;
class UnifiedMessageCenterBubble;
class UnifiedSystemTrayModel;
class UnifiedSystemTrayView;

// TODO(b:252887883): Clean up old animation code once the new clear all
// animation is implemented.

// Note: This enum represents the current animation
// state for `NotificationCenterView`. There is an equivalent animation state
// enum in the child `NotificationListView`. The animations for these two views
// can occur simultaneously or independently, so states for both views are
// tracked separately.
enum class NotificationCenterAnimationState {
  // No animation is running.
  IDLE,

  // Animating hiding the stacking bar. Runs when the user dismisses the
  // second to last notification and during the clear all animation.
  HIDE_STACKING_BAR,

  // Animating collapsing the entire message center. Runs after the user
  // dismisses the last notification and during the clear all animation.
  COLLAPSE,
};

// Manages scrolling of notification list.
class ASH_EXPORT NotificationCenterView
    : public views::View,
      public MessageCenterScrollBar::Observer,
      public views::FocusChangeListener,
      public gfx::AnimationDelegate,
      public views::ViewObserver {
 public:
  METADATA_HEADER(NotificationCenterView);

  explicit NotificationCenterView(
      UnifiedSystemTrayView* parent = nullptr,
      scoped_refptr<UnifiedSystemTrayModel> model = nullptr,
      UnifiedMessageCenterBubble* bubble = nullptr);

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

  // Sets the maximum height that the view can take.
  // TODO(tengs): The layout of this view is heavily dependant on this max
  // height (equal to the height of the entire tray), but we should refactor and
  // consolidate this function with `SetAvailableHeight()`.
  void SetMaxHeight(int max_height);

  // Sets the height available to the message center view. This is the remaining
  // height after counting the system menu, which may be expanded or collapsed.
  void SetAvailableHeight(int available_height);

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

  // Relinquish focus and transfer it to the quick settings widget.
  void FocusOut(bool reverse);

  // Set the first notification view to be focused when focus is acquired.
  // This is the oldest notification if `reverse` is `true`. Otherwise, if
  // `reverse` is `false`, this is the newest notification.
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

  // Returns true if the scroll bar is visible.
  bool IsScrollBarVisible() const;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // MessageCenterScrollBar::Observer:
  void OnMessageCenterScrolled() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  NotificationListView* notification_list_view() {
    return notification_list_view_;
  }
  bool collapsed() { return collapsed_; }

 private:
  friend class NotificationCenterTestApi;
  friend class NotificationCenterViewTest;
  friend class UnifiedMessageCenterBubbleTest;

  // Callback for whenever the scroll view contained in this view receives a
  // scroll event.
  void OnContentsScrolled();

  // Starts the animation to hide the `StackedNotificationBar`.
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

  // Get first and last focusable child views. These functions are used to
  // figure out if we need to focus out or to set the correct focused view
  // when focus is acquired from another widget.
  View* GetFirstFocusableChild();
  View* GetLastFocusableChild();

  const raw_ptr<UnifiedSystemTrayView, ExperimentalAsh> parent_;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  const raw_ptr<UnifiedMessageCenterBubble, ExperimentalAsh>
      message_center_bubble_;
  const raw_ptr<StackedNotificationBar, ExperimentalAsh> notification_bar_;
  raw_ptr<views::ScrollBar, ExperimentalAsh> scroll_bar_;
  const raw_ptr<views::ScrollView, ExperimentalAsh> scroller_;
  const raw_ptr<NotificationListView, ExperimentalAsh> notification_list_view_;

  raw_ptr<views::BoxLayout, ExperimentalAsh> layout_manager_ = nullptr;

  // Position from the bottom of scroll contents in dip.
  int last_scroll_position_from_bottom_;

  // The height available to the message center view. This is the remaining
  // height of the system tray excluding the system menu (which can be expanded
  // or collapsed).
  int available_height_ = 0;

  // Current state of the message center view.
  bool collapsed_ = false;

  // Tracks the current animation state.
  NotificationCenterAnimationState animation_state_ =
      NotificationCenterAnimationState::IDLE;
  const std::unique_ptr<gfx::LinearAnimation> animation_;

  const std::unique_ptr<views::FocusSearch> focus_search_;

  raw_ptr<views::FocusManager, ExperimentalAsh> focus_manager_ = nullptr;

  base::CallbackListSubscription on_contents_scrolled_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_VIEW_H_
