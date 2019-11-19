// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_view.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/stacked_notification_bar.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr base::TimeDelta kHideStackingBarAnimationDuration =
    base::TimeDelta::FromMilliseconds(330);
constexpr base::TimeDelta kCollapseAnimationDuration =
    base::TimeDelta::FromMilliseconds(640);

class ScrollerContentsView : public views::View {
 public:
  ScrollerContentsView(UnifiedMessageListView* message_list_view) {
    int bottom_padding = features::IsUnifiedMessageCenterRefactorEnabled()
                             ? 0
                             : kUnifiedNotificationCenterSpacing;

    auto* contents_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(0, 0, bottom_padding, 0)));
    contents_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    AddChildView(message_list_view);
  }

  ~ScrollerContentsView() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* view) override {
    PreferredSizeChanged();
  }

  const char* GetClassName() const override { return "ScrollerContentsView"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollerContentsView);
};

}  // namespace

UnifiedMessageCenterView::UnifiedMessageCenterView(
    UnifiedSystemTrayView* parent,
    UnifiedSystemTrayModel* model,
    UnifiedMessageCenterBubble* bubble)
    : parent_(parent),
      model_(model),
      message_center_bubble_(bubble),
      notification_bar_(new StackedNotificationBar(this)),
      scroll_bar_(new MessageCenterScrollBar(this)),
      scroller_(new views::ScrollView()),
      message_list_view_(new UnifiedMessageListView(this, model)),
      last_scroll_position_from_bottom_(0),
      animation_(std::make_unique<gfx::LinearAnimation>(this)),
      focus_search_(std::make_unique<views::FocusSearch>(this, false, false)) {
  message_list_view_->Init();

  AddChildView(notification_bar_);

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetContents(
      std::make_unique<ScrollerContentsView>(message_list_view_));
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(base::WrapUnique(scroll_bar_));
  scroller_->SetDrawOverflowIndicator(false);
  AddChildView(scroller_);

  notification_bar_->Update(message_list_view_->GetTotalNotificationCount(),
                            GetStackedNotifications());
}

UnifiedMessageCenterView::~UnifiedMessageCenterView() {
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);

  RemovedFromWidget();
}

void UnifiedMessageCenterView::SetMaxHeight(int max_height) {
  int max_scroller_height = max_height;
  if (notification_bar_->GetVisible())
    max_scroller_height -= kStackedNotificationBarHeight;
  scroller_->ClipHeightTo(0, max_scroller_height);
}

void UnifiedMessageCenterView::SetAvailableHeight(int available_height) {
  available_height_ = available_height;
  UpdateVisibility();
}

void UnifiedMessageCenterView::SetExpanded() {
  if (!GetVisible() || !collapsed_)
    return;

  collapsed_ = false;
  notification_bar_->SetExpanded();
  scroller_->SetVisible(true);
  Layout();
}

void UnifiedMessageCenterView::SetCollapsed(bool animate) {
  if (!GetVisible() || collapsed_)
    return;

  collapsed_ = true;
  if (animate) {
    StartCollapseAnimation();
  } else {
    scroller_->SetVisible(false);
    notification_bar_->SetCollapsed();
  }
}

void UnifiedMessageCenterView::ClearAllNotifications() {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_StackingBarClearAll"));

  message_list_view_->ClearAllWithAnimation();
}

void UnifiedMessageCenterView::ExpandMessageCenter() {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_SeeAllNotifications"));
  message_center_bubble_->ExpandMessageCenter();
}

void UnifiedMessageCenterView::OnNotificationSlidOut() {
  if (notification_bar_->GetVisible() &&
      message_list_view_->GetTotalNotificationCount() <= 1) {
    StartHideStackingBarAnimation();
  } else if (!message_list_view_->GetTotalNotificationCount()) {
    StartCollapseAnimation();
  }
}

void UnifiedMessageCenterView::ListPreferredSizeChanged() {
  UpdateVisibility();
  PreferredSizeChanged();

  if (features::IsUnifiedMessageCenterRefactorEnabled())
    SetMaxHeight(available_height_);

  Layout();

  if (GetWidget() && !GetWidget()->IsClosed())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void UnifiedMessageCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void UnifiedMessageCenterView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void UnifiedMessageCenterView::RemovedFromWidget() {
  if (!focus_manager_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void UnifiedMessageCenterView::Layout() {
  if (notification_bar_->GetVisible()) {
    gfx::Rect counter_bounds(GetContentsBounds());

    int notification_bar_height = collapsed_
                                      ? kStackedNotificationBarCollapsedHeight
                                      : kStackedNotificationBarHeight;
    int notification_bar_offset = 0;
    if (animation_state_ ==
        UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR)
      notification_bar_offset = GetAnimationValue() * notification_bar_height;

    counter_bounds.set_height(notification_bar_height);
    counter_bounds.set_y(counter_bounds.y() - notification_bar_offset);
    notification_bar_->SetBoundsRect(counter_bounds);

    gfx::Rect scroller_bounds(GetContentsBounds());
    scroller_bounds.Inset(gfx::Insets(
        notification_bar_height - notification_bar_offset, 0, 0, 0));
    scroller_->SetBoundsRect(scroller_bounds);
  } else {
    scroller_->SetBoundsRect(GetContentsBounds());
  }

  ScrollToTarget();
  NotifyRectBelowScroll();
}

gfx::Size UnifiedMessageCenterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = scroller_->GetPreferredSize();

  if (notification_bar_->GetVisible()) {
    int bar_height = kStackedNotificationBarHeight;
    if (animation_state_ ==
        UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR)
      bar_height -= GetAnimationValue() * bar_height;
    preferred_size.set_height(preferred_size.height() + bar_height);
  }

  if (animation_state_ == UnifiedMessageCenterAnimationState::COLLAPSE) {
    int height = preferred_size.height() * (1.0 - GetAnimationValue());

    if (collapsed_)
      height = std::max(kStackedNotificationBarCollapsedHeight, height);

    preferred_size.set_height(height);
  } else if (collapsed_) {
    preferred_size.set_height(kStackedNotificationBarCollapsedHeight);
  }

  return preferred_size;
}

const char* UnifiedMessageCenterView::GetClassName() const {
  return "UnifiedMessageCenterView";
}

void UnifiedMessageCenterView::OnMessageCenterScrolled() {
  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();

  // Reset the target if user scrolls the list manually.
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION);

  bool was_count_updated =
      notification_bar_->Update(message_list_view_->GetTotalNotificationCount(),
                                GetStackedNotifications());
  if (was_count_updated) {
    const int previous_y = scroller_->y();
    Layout();
    // Adjust scroll position when counter visibility is changed so that
    // on-screen position of notification list does not change.
    scroll_bar_->ScrollByContentsOffset(previous_y - scroller_->y());
  }

  NotifyRectBelowScroll();
}

void UnifiedMessageCenterView::OnWillChangeFocus(views::View* before,
                                                 views::View* now) {}

void UnifiedMessageCenterView::OnDidChangeFocus(views::View* before,
                                                views::View* now) {
  if (message_list_view_->is_deleting_removed_notifications())
    return;

  OnMessageCenterScrolled();

  if (features::IsUnifiedMessageCenterRefactorEnabled()) {
    views::View* first_view = GetFirstFocusableChild();
    views::View* last_view = GetLastFocusableChild();

    // If we are cycling back to the first view from the last view or vice
    // verse. Focus out of the message center to the quick settings bubble. The
    // direction of the cycle determines where the focus will move to in quick
    // settings.
    bool focused_out = false;
    if (before == last_view && now == first_view)
      focused_out = message_center_bubble_->FocusOut(false /* reverse */);
    else if (before == first_view && now == last_view)
      focused_out = message_center_bubble_->FocusOut(true /* reverse */);

    // Clear the focus state completely for the message center.
    // We acquire the focus back from the quick settings widget based on the
    // cycling direction.
    if (focused_out) {
      GetFocusManager()->ClearFocus();
      GetFocusManager()->SetStoredFocusView(nullptr);
    }
  }
}

void UnifiedMessageCenterView::AnimationEnded(const gfx::Animation* animation) {
  // This is also called from AnimationCanceled().
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  switch (animation_state_) {
    case UnifiedMessageCenterAnimationState::IDLE:
      break;
    case UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR:
      break;
    case UnifiedMessageCenterAnimationState::COLLAPSE:
      break;
  }

  animation_state_ = UnifiedMessageCenterAnimationState::IDLE;
  notification_bar_->SetAnimationState(animation_state_);
  UpdateVisibility();
}

void UnifiedMessageCenterView::AnimationProgressed(
    const gfx::Animation* animation) {
  // Make the scroller containing notifications invisible and change the
  // notification bar to it's collapsed state in the middle of the animation to
  // the collapsed state.
  if (collapsed_ && scroller_->GetVisible() &&
      animation_->GetCurrentValue() >= 0.5) {
    scroller_->SetVisible(false);
    notification_bar_->SetCollapsed();
  }
  PreferredSizeChanged();
}

void UnifiedMessageCenterView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void UnifiedMessageCenterView::SetNotificationRectBelowScroll(
    const gfx::Rect& rect_below_scroll) {
  parent_->SetNotificationRectBelowScroll(rect_below_scroll);
}

void UnifiedMessageCenterView::StartHideStackingBarAnimation() {
  animation_->End();
  animation_state_ = UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR;
  notification_bar_->SetAnimationState(animation_state_);
  animation_->SetDuration(kHideStackingBarAnimationDuration);
  animation_->Start();
}

void UnifiedMessageCenterView::StartCollapseAnimation() {
  animation_->End();
  animation_state_ = UnifiedMessageCenterAnimationState::COLLAPSE;
  notification_bar_->SetAnimationState(animation_state_);
  animation_->SetDuration(kCollapseAnimationDuration);
  animation_->Start();
}

double UnifiedMessageCenterView::GetAnimationValue() const {
  return gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                    animation_->GetCurrentValue());
}

void UnifiedMessageCenterView::UpdateVisibility() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  SetVisible(
      available_height_ >= kUnifiedNotificationMinimumHeight &&
      (animation_state_ == UnifiedMessageCenterAnimationState::COLLAPSE ||
       message_list_view_->GetPreferredSize().height() > 0) &&
      session_controller->ShouldShowNotificationTray() &&
      (!session_controller->IsScreenLocked() ||
       AshMessageCenterLockScreenController::IsEnabled()));

  // When notification list went invisible, the last notification should be
  // targeted next time.
  if (!GetVisible()) {
    model_->set_notification_target_mode(
        UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);
    NotifyRectBelowScroll();
  }
}

void UnifiedMessageCenterView::ScrollToTarget() {
  // Following logic doesn't work when the view is invisible, because it uses
  // the height of |scroller_|.
  if (!GetVisible())
    return;

  auto target_mode = model_->notification_target_mode();

  // Notification views may be deleted during an animation, so wait until it
  // finishes before scrolling to a new target (see crbug.com/954001).
  if (message_list_view_->IsAnimating())
    target_mode = UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION;

  int position;
  switch (target_mode) {
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION:
      // Restore the previous scrolled position with matching the distance from
      // the bottom.
      position =
          scroll_bar_->GetMaxPosition() - last_scroll_position_from_bottom_;
      break;
    case UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID:
      FALLTHROUGH;
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION: {
      const gfx::Rect& target_rect =
          (model_->notification_target_mode() ==
           UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID)
              ? message_list_view_->GetNotificationBounds(
                    model_->notification_target_id())
              : message_list_view_->GetLastNotificationBounds();

      const int last_notification_offset = target_rect.height() -
                                           scroller_->height() +
                                           kUnifiedNotificationCenterSpacing;
      if (last_notification_offset > 0) {
        // If the target notification is taller than |scroller_|, we should
        // align the top of the notification with the top of |scroller_|.
        position = target_rect.y();
      } else {
        // Otherwise, we align the bottom of the notification with the bottom of
        // |scroller_|;
        position = target_rect.bottom() - scroller_->height();

        if (model_->notification_target_mode() ==
            UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION) {
          position += kUnifiedNotificationCenterSpacing;
        }
      }
    }
  }

  scroller_->ScrollToPosition(scroll_bar_, position);
  notification_bar_->Update(message_list_view_->GetTotalNotificationCount(),
                            GetStackedNotifications());
  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();
}

std::vector<message_center::Notification*>
UnifiedMessageCenterView::GetStackedNotifications() const {
  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty())
    scroller_->SetBoundsRect(GetContentsBounds());

  // Use this y offset to count number of hidden notifications.
  // Set to the bottom of the last notification when message center is
  // collapsed. Set below stacked notification bar when message center is
  // expanded.
  int y_offset;
  if (collapsed_) {
    gfx::Rect last_bounds = message_list_view_->GetLastNotificationBounds();
    y_offset = last_bounds.y() + last_bounds.height();
  } else {
    y_offset = scroller_->GetVisibleRect().y() - scroller_->y() +
               kStackedNotificationBarHeight;
  }
  return message_list_view_->GetNotificationsAboveY(y_offset);
}

void UnifiedMessageCenterView::NotifyRectBelowScroll() {
  if (features::IsUnifiedMessageCenterRefactorEnabled())
    return;
  // If the message center is hidden, make sure rounded corners are not drawn.
  if (!GetVisible()) {
    SetNotificationRectBelowScroll(gfx::Rect());
    return;
  }

  gfx::Rect rect_below_scroll;
  rect_below_scroll.set_height(
      std::max(0, message_list_view_->GetLastNotificationBounds().bottom() -
                      scroller_->GetVisibleRect().bottom()));

  gfx::Rect notification_bounds =
      message_list_view_->GetNotificationBoundsBelowY(
          scroller_->GetVisibleRect().bottom());
  rect_below_scroll.set_x(notification_bounds.x());
  rect_below_scroll.set_width(notification_bounds.width());

  SetNotificationRectBelowScroll(rect_below_scroll);
}

void UnifiedMessageCenterView::FocusOut(bool reverse) {
  message_center_bubble_->FocusOut(reverse);
}

void UnifiedMessageCenterView::FocusEntered(bool reverse) {
  views::View* focus_view =
      reverse ? GetLastFocusableChild() : GetFirstFocusableChild();
  GetFocusManager()->SetFocusedView(focus_view);
}

views::View* UnifiedMessageCenterView::GetFirstFocusableChild() {
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

views::View* UnifiedMessageCenterView::GetLastFocusableChild() {
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kBackwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

}  // namespace ash
