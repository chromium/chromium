// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_list_view.h"

#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_center_view.h"
#include "ash/system/message_center/slidable_message_view.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using message_center::MessageView;
using message_center::SlidableMessageView;
using message_center::Notification;

namespace ash {

namespace {
const int kAnimateClearingNextNotificationDelayMS = 40;
}  // namespace

MessageListView::MessageListView()
    : reposition_top_(-1),
      fixed_height_(0),
      has_deferred_task_(false),
      clear_all_started_(false),
      use_fixed_height_(true),
      has_border_padding_(false),
      animator_(this),
      weak_ptr_factory_(this) {
  auto layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical,
                                                   gfx::Insets(), 1);
  layout->SetDefaultFlex(1);
  SetLayoutManager(std::move(layout));

  animator_.AddObserver(this);
}

MessageListView::~MessageListView() {
  animator_.RemoveObserver(this);
}

void MessageListView::Layout() {
  if (animator_.IsAnimating())
    return;

  gfx::Rect child_area = GetContentsBounds();
  int top = child_area.y();

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    int height = child->GetHeightForWidth(child_area.width());
    child->SetBounds(child_area.x(), top, child_area.width(), height);
    top += height + GetMarginBetweenItems();
  }
}

void MessageListView::AddNotificationAt(MessageView* message_view, int index) {
  // |index| refers to a position in a subset of valid children. |real_index|
  // in a list includes the invalid children, so we compute the real index by
  // walking the list until |index| number of valid children are encountered,
  // or to the end of the list.
  int real_index = 0;
  while (real_index < child_count()) {
    if (IsValidChild(child_at(real_index))) {
      --index;
      if (index < 0)
        break;
    }
    ++real_index;
  }

  if (real_index == 0)
    ExpandSpecifiedNotificationAndCollapseOthers(message_view);

  SlidableMessageView* container = new SlidableMessageView(message_view);
  AddChildViewAt(container, real_index);
  if (GetContentsBounds().IsEmpty())
    return;

  adding_views_.insert(container);
  DoUpdateIfPossible();
}

void MessageListView::RemoveNotification(MessageView* message_view) {
  views::View* container = message_view->parent();
  DCHECK_EQ(container->parent(), this);

  // TODO(yhananda): We should consider consolidating clearing_all_views_,
  // deleting_views_ and deleted_when_done_.
  if (base::ContainsValue(clearing_all_views_, container) ||
      deleting_views_.find(container) != deleting_views_.end() ||
      deleted_when_done_.find(container) != deleted_when_done_.end()) {
    // Let's skip deleting the view if it's already scheduled for deleting.
    // Even if we check clearing_all_views_ here, we actualy have no idea
    // whether the view is due to be removed or not because it could be in its
    // animation before removal.
    // In short, we could delete the view twice even if we check these three
    // lists.
    return;
  }

  if (GetContentsBounds().IsEmpty()) {
    delete container;
  } else {
    if (adding_views_.find(container) != adding_views_.end())
      adding_views_.erase(container);
    if (animator_.IsAnimating(container))
      animator_.StopAnimatingView(container);

    if (container->layer()) {
      deleting_views_.insert(container);
    } else {
      delete container;
    }
    DoUpdateIfPossible();
  }

  int index = GetIndexOf(container);
  if (index == 0)
    ExpandTopNotificationAndCollapseOthers();
}

void MessageListView::UpdateNotification(MessageView* message_view,
                                         const Notification& notification) {
  auto* container = SlidableMessageView::GetFromMessageView(message_view);

  // Skip updating the notification being cleared.
  if (base::ContainsValue(clearing_all_views_, container))
    return;

  int index = GetIndexOf(container);
  DCHECK_LE(0, index);  // GetIndexOf is negative if not a child.

  if (index == 0)
    ExpandSpecifiedNotificationAndCollapseOthers(message_view);

  animator_.StopAnimatingView(container);
  if (deleting_views_.find(container) != deleting_views_.end())
    deleting_views_.erase(container);
  if (deleted_when_done_.find(container) != deleted_when_done_.end())
    deleted_when_done_.erase(container);
  container->UpdateWithNotification(notification);
  DoUpdateIfPossible();
}

std::pair<int, MessageView*> MessageListView::GetNotificationById(
    const std::string& id) {
  for (int i = child_count() - 1; i >= 0; --i) {
    DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
              child_at(i)->GetClassName());
    SlidableMessageView* container =
        static_cast<SlidableMessageView*>(child_at(i));
    if (container->notification_id() == id && IsValidChild(container))
      return std::make_pair(i, container->GetMessageView());
  }
  return std::make_pair(-1, nullptr);
}

MessageView* MessageListView::GetNotificationAt(int index) {
  for (int i = child_count() - 1; i >= 0; --i) {
    DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
              child_at(i)->GetClassName());
    SlidableMessageView* container =
        static_cast<SlidableMessageView*>(child_at(i));
    if (IsValidChild(container)) {
      if (index == 0)
        return container->GetMessageView();
      index--;
    }
  }
  return nullptr;
}

size_t MessageListView::GetNotificationCount() const {
  int count = 0;
  for (int i = child_count() - 1; i >= 0; --i) {
    const SlidableMessageView* container =
        static_cast<const SlidableMessageView*>(child_at(i));
    if (IsValidChild(container))
      count++;
  }
  return count;
}

gfx::Size MessageListView::CalculatePreferredSize() const {
  // Just returns the current size. All size change must be done in
  // |DoUpdateIfPossible()| with animation , because we don't want to change
  // the size in unexpected timing.
  return size();
}

int MessageListView::GetHeightForWidth(int width) const {
  if (use_fixed_height_ && fixed_height_ > 0)
    return fixed_height_;

  width -= GetInsets().width();
  int height = 0;
  int padding = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (!IsValidChild(child))
      continue;
    height += child->GetHeightForWidth(width) + padding;
    padding = GetMarginBetweenItems();
  }

  return height + GetInsets().height();
}

void MessageListView::PaintChildren(const views::PaintInfo& paint_info) {
  // Paint in the inversed order. Otherwise upper notification may be
  // hidden by the lower one.
  for (int i = child_count() - 1; i >= 0; --i) {
    if (!child_at(i)->layer())
      child_at(i)->Paint(paint_info);
  }
}

void MessageListView::ReorderChildLayers(ui::Layer* parent_layer) {
  // Reorder children to stack the last child layer at the top. Otherwise
  // upper notification may be hidden by the lower one.
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->layer())
      parent_layer->StackAtBottom(child_at(i)->layer());
  }
}

void MessageListView::UpdateFixedHeight(int requested_height,
                                        bool prevent_scroll) {
  int previous_fixed_height = fixed_height_;
  int min_height;

  // When the |prevent_scroll| flag is set, we use |fixed_height_|, which is the
  // bottom position of the visible rect. It's to keep the current visible
  // window, in other words, not to be scrolled, when the visible rect has a
  // blank area at the bottom.
  // Otherwise (in else block), we use the height of the visible rect to make
  // the height of the message list as small as possible.
  if (prevent_scroll) {
    // TODO(yoshiki): Consider the case with scrolling. If the message center
    // has scrollbar and its height is maximum, we may not need to keep the
    // height of the list in the scroll view.
    min_height = fixed_height_;
  } else {
    if (scroller_) {
      gfx::Rect visible_rect = scroller_->GetVisibleRect();
      min_height = visible_rect.height();
    } else {
      // Fallback for testing.
      min_height = fixed_height_;
    }
  }
  fixed_height_ = std::max(min_height, requested_height);

  if (previous_fixed_height != fixed_height_) {
    PreferredSizeChanged();
  }
}

void MessageListView::SetRepositionTarget(const gfx::Rect& target) {
  reposition_top_ = std::max(target.y(), 0);
  UpdateFixedHeight(GetHeightForWidth(width()), false);
}

void MessageListView::ResetRepositionSession() {
  // Don't call DoUpdateIfPossible(), but let Layout() do the task without
  // animation. Reset will cause the change of the bubble size itself, and
  // animation from the old location will look weird.
  if (reposition_top_ >= 0) {
    has_deferred_task_ = false;
    // cancel cause OnBoundsAnimatorDone which deletes |deleted_when_done_|.
    animator_.Cancel();
    for (auto* view : deleting_views_)
      delete view;
    deleting_views_.clear();
    adding_views_.clear();
  }

  reposition_top_ = -1;

  UpdateFixedHeight(fixed_height_, false);
}

void MessageListView::ClearAllClosableNotifications(
    const gfx::Rect& visible_scroll_rect) {
  for (int i = 0; i < child_count(); ++i) {
    // Safe cast since all views in MessageListView are SlidableMessageViews.
    DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
              child_at(i)->GetClassName());
    SlidableMessageView* child = static_cast<SlidableMessageView*>(child_at(i));
    if (!child->visible())
      continue;
    if (gfx::IntersectRects(child->bounds(), visible_scroll_rect).IsEmpty())
      continue;
    if (child->GetMode() != MessageView::Mode::NORMAL)
      continue;
    if (deleting_views_.find(child) != deleting_views_.end() ||
        deleted_when_done_.find(child) != deleted_when_done_.end()) {
      // We don't check clearing_all_views_ here, so this can lead to a
      // notification being deleted twice. Even if we do check it, there is a
      // problem similar to the problem in RemoveNotification(), it could be
      // currently in its animation before removal, and we could similarly
      // delete it twice. This is a bug.
      continue;
    }
    clearing_all_views_.push_back(child);
  }
  if (clearing_all_views_.empty()) {
    for (auto& observer : observers_)
      observer.OnAllNotificationsCleared();
  } else {
    DoUpdateIfPossible();
  }
}

void MessageListView::AddObserver(MessageListView::Observer* observer) {
  observers_.AddObserver(observer);
}

void MessageListView::RemoveObserver(MessageListView::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessageListView::SetBorderPadding() {
  has_border_padding_ = true;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(message_center::kMarginBetweenItemsInList)));
}

int MessageListView::GetCountAboveVisibleRect(int y_offset) const {
  DCHECK(scroller_);

  int height = 0;
  int padding = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (!child->visible())
      continue;
    if (!IsValidChild(child))
      continue;

    height += child->bounds().height() + padding;
    padding = GetMarginBetweenItems();

    if (height >= scroller_->GetVisibleRect().y() + y_offset)
      return i;
  }
  return child_count();
}

int MessageListView::GetHeightBelowVisibleRect() const {
  DCHECK(scroller_);

  int height = 0;
  int padding = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (!child->visible())
      continue;
    if (!IsValidChild(child))
      continue;

    height += child->bounds().height() + padding;
    padding = GetMarginBetweenItems();
  }
  return std::max(0, height - scroller_->GetVisibleRect().bottom());
}

void MessageListView::OnBoundsAnimatorProgressed(
    views::BoundsAnimator* animator) {
  DCHECK_EQ(&animator_, animator);
  for (auto* view : deleted_when_done_) {
    const gfx::SlideAnimation* animation = animator->GetAnimationForView(view);
    if (animation)
      view->layer()->SetOpacity(animation->CurrentValueBetween(1.0, 0.0));
  }
}

void MessageListView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  // It's possible for the delayed task that queues the next animation for
  // clearing all notifications to be delayed more than we want. In this case,
  // the BoundsAnimator can finish while a clear all is still happening. So,
  // explicitly check if |clearing_all_views_| is empty.
  if (clear_all_started_ && !clearing_all_views_.empty()) {
    return;
  }

  bool need_update = false;

  if (clear_all_started_) {
    clear_all_started_ = false;
    for (auto& observer : observers_)
      observer.OnAllNotificationsCleared();

    // Just return here if new animation is initiated in the above observers,
    // since the code below assumes no animation is running. In the current
    // impelementation, the observer tries removing the notification and their
    // views and starts animation if the message center keeps opening.
    // The code below will be executed when the new animation is finished.
    if (animator_.IsAnimating())
      return;
  }

  // None of these views should be deleted.
  DCHECK(std::all_of(deleted_when_done_.begin(), deleted_when_done_.end(),
                     [this](views::View* view) { return Contains(view); }));

  for (auto* view : deleted_when_done_)
    delete view;
  deleted_when_done_.clear();

  if (has_deferred_task_) {
    has_deferred_task_ = false;
    need_update = true;
  }

  if (need_update)
    DoUpdateIfPossible();

  if (GetWidget() && !GetWidget()->IsClosed())
    GetWidget()->SynthesizeMouseMoveEvent();
}

int MessageListView::GetMarginBetweenItems() const {
  return has_border_padding_ ? message_center::kMarginBetweenItemsInList : 0;
}

bool MessageListView::IsValidChild(const views::View* child) const {
  return deleting_views_.find(const_cast<views::View*>(child)) ==
             deleting_views_.end() &&
         deleted_when_done_.find(const_cast<views::View*>(child)) ==
             deleted_when_done_.end() &&
         !base::ContainsValue(clearing_all_views_, child) && child->visible();
}

void MessageListView::DoUpdateIfPossible() {
  gfx::Rect child_area = GetContentsBounds();
  if (child_area.IsEmpty())
    return;

  if (animator_.IsAnimating()) {
    has_deferred_task_ = true;
    return;
  }

  // Start the clearing all animation if necessary.
  if (!clearing_all_views_.empty() && !clear_all_started_) {
    AnimateClearingOneNotification();
    return;
  }

  // Skip during the clering all animation.
  // This checks |clear_all_started_! rather than |clearing_all_views_|, since
  // the latter is empty during the animation of the last element.
  if (clear_all_started_) {
    DCHECK(!clearing_all_views_.empty());
    return;
  }

  AnimateNotifications();

  // Should calculate and set new size after calling AnimateNotifications()
  // because fixed_height_ may be updated in it.
  int new_height = GetHeightForWidth(child_area.width() + GetInsets().width());
  SetSize(gfx::Size(child_area.width() + GetInsets().width(), new_height));

  adding_views_.clear();
  deleting_views_.clear();

  if (!animator_.IsAnimating() && GetWidget() && !GetWidget()->IsClosed())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void MessageListView::ExpandSpecifiedNotificationAndCollapseOthers(
    message_center::MessageView* target_view) {
  if (!target_view->IsManuallyExpandedOrCollapsed() &&
      target_view->IsAutoExpandingAllowed()) {
    target_view->SetExpanded(true);
  }

  for (int i = 0; i < child_count(); ++i) {
    DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
              child_at(i)->GetClassName());
    SlidableMessageView* container =
        static_cast<SlidableMessageView*>(child_at(i));
    // Target view is already processed above.
    if (target_view == container->GetMessageView())
      continue;
    // Skip if the view is invalid.
    if (!IsValidChild(container))
      continue;
    // We don't touch if the view has been manually expanded or collapsed.
    if (container->IsManuallyExpandedOrCollapsed())
      continue;

    // Otherwise, collapse the notification.
    container->SetExpanded(false);
  }
}

void MessageListView::ExpandTopNotificationAndCollapseOthers() {
  SlidableMessageView* top_notification = nullptr;
  for (int i = 0; i < child_count(); ++i) {
    DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
              child_at(i)->GetClassName());
    SlidableMessageView* container =
        static_cast<SlidableMessageView*>(child_at(i));
    if (!IsValidChild(container))
      continue;
    top_notification = container;
    break;
  }

  if (top_notification != nullptr)
    ExpandSpecifiedNotificationAndCollapseOthers(
        top_notification->GetMessageView());
}

std::vector<int> MessageListView::ComputeRepositionOffsets(
    const std::vector<int>& heights,
    const std::vector<bool>& deleting,
    int target_index,
    int padding) {
  DCHECK_EQ(heights.size(), deleting.size());
  // Calculate the vertical length between the top of message list and the top
  // of target. This is to shrink or expand the height of the message list
  // when the notifications above the target is changed.
  int vertical_gap_to_target_from_top = GetInsets().top();
  for (int i = 0; i < target_index; i++) {
    if (!deleting[i])
      vertical_gap_to_target_from_top += heights[i] + padding;
  }

  // If the calculated length is expanded from |repositon_top_|, it means that
  // some of items above the target are updated and their height increased.
  // Adjust the vertical length above the target.
  if (vertical_gap_to_target_from_top > reposition_top_) {
    fixed_height_ += vertical_gap_to_target_from_top - reposition_top_;
    reposition_top_ = vertical_gap_to_target_from_top;
  }

  // TODO(yoshiki): Scroll the parent container to keep the physical position
  // of the target notification when the scrolling is caused by a size change
  // of notification above.

  std::vector<int> positions;
  positions.reserve(heights.size());
  // Layout the items above the target.
  int y = GetInsets().top();
  for (int i = 0; i < target_index; i++) {
    positions.push_back(y);
    if (!deleting[i])
      y += heights[i] + padding;
  }
  DCHECK_EQ(y, vertical_gap_to_target_from_top);
  DCHECK_LE(y, reposition_top_);

  // Match the top with |reposition_top_|.
  y = reposition_top_;
  // Layout the target and the items below the target.
  for (int i = target_index; i < int(heights.size()); i++) {
    positions.push_back(y);
    if (!deleting[i])
      y += heights[i] + padding;
  }

  // Update the fixed height. |requested_height| is the height to have all
  // notifications in the list and to keep the vertical position of the target
  // notification. It may not just a total of all the notification heights if
  // the target exists.
  int requested_height = y - padding + GetInsets().bottom();
  UpdateFixedHeight(requested_height, true);

  return positions;
}

void MessageListView::AnimateNotifications() {
  int target_index = -1;
  int padding = GetMarginBetweenItems();
  gfx::Rect child_area = GetContentsBounds();
  if (reposition_top_ >= 0) {
    // Find the target item.
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      if (child->y() >= reposition_top_ &&
          deleting_views_.find(child) == deleting_views_.end()) {
        // Find the target.
        target_index = i;
        break;
      }
    }
  }

  if (target_index != -1) {
    std::vector<int> heights;
    std::vector<bool> deleting;
    heights.reserve(child_count());
    deleting.reserve(child_count());
    for (int i = 0; i < child_count(); i++) {
      views::View* child = child_at(i);
      heights.push_back(child->GetHeightForWidth(child_area.width()));
      deleting.push_back(deleting_views_.find(child) != deleting_views_.end());
    }
    std::vector<int> ys =
        ComputeRepositionOffsets(heights, deleting, target_index, padding);
    for (int i = 0; i < child_count(); ++i) {
      bool above_target = (i < target_index);
      AnimateChild(child_at(i), ys[i], heights[i],
                   !above_target /* animate_on_move */);
    }
  } else {
    // Layout all the items.
    int y = GetInsets().top();
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      int height = child->GetHeightForWidth(child_area.width());
      if (AnimateChild(child, y, height, true))
        y += height + padding;
    }
    int new_height = y - padding + GetInsets().bottom();
    UpdateFixedHeight(new_height, false);
  }
}

bool MessageListView::AnimateChild(views::View* child,
                                   int top,
                                   int height,
                                   bool animate_on_move) {
  // Do not call this during clearing all animation.
  DCHECK(clearing_all_views_.empty());
  DCHECK(!clear_all_started_);

  gfx::Rect child_area = GetContentsBounds();
  if (adding_views_.find(child) != adding_views_.end()) {
    child->SetBounds(child_area.right(), top, child_area.width(), height);
    animator_.AnimateViewTo(
        child, gfx::Rect(child_area.x(), top, child_area.width(), height));
  } else if (deleting_views_.find(child) != deleting_views_.end()) {
    DCHECK(child->layer());
    // No moves, but animate to fade-out.
    animator_.AnimateViewTo(child, child->bounds());
    deleted_when_done_.insert(child);
    return false;
  } else {
    gfx::Rect target(child_area.x(), top, child_area.width(), height);
    if (child->origin() != target.origin() && animate_on_move)
      animator_.AnimateViewTo(child, target);
    else
      child->SetBoundsRect(target);
  }
  return true;
}

void MessageListView::AnimateClearingOneNotification() {
  DCHECK(!clearing_all_views_.empty());

  clear_all_started_ = true;

  views::View* child = clearing_all_views_.front();
  clearing_all_views_.pop_front();

  // Slide from left to right.
  gfx::Rect new_bounds = child->bounds();
  new_bounds.set_x(new_bounds.right() + GetMarginBetweenItems());
  animator_.AnimateViewTo(child, new_bounds);

  // Schedule to start sliding out next notification after a short delay.
  if (!clearing_all_views_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MessageListView::AnimateClearingOneNotification,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAnimateClearingNextNotificationDelayMS));
  }
}

void MessageListView::OnSlideChanged(const std::string& notification_id) {
  for (int i = 0; i < child_count(); ++i) {
    DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
              child_at(i)->GetClassName());
    SlidableMessageView* container =
        static_cast<SlidableMessageView*>(child_at(i));
    MessageView* message = container->GetMessageView();
    if (message->notification_id() == notification_id)
      continue;
    if (!IsValidChild(container))
      continue;
    container->CloseSwipeControl();
  }
}

void MessageListView::UpdateCornerRadius(int index,
                                         int top_radius,
                                         int bottom_radius) {
  auto* message_view = GetNotificationAt(index);
  auto* container = SlidableMessageView::GetFromMessageView(message_view);

  message_view->UpdateCornerRadius(top_radius, bottom_radius);
  container->UpdateCornerRadius(top_radius, bottom_radius);
}

}  // namespace ash
