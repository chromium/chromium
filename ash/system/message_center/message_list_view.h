// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_LIST_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_LIST_VIEW_H_

#include <list>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/message_center/slidable_message_view.h"
#include "base/macros.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ui {
class Layer;
}

namespace message_center {
class SlidableMessageView;
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

// Displays a list of messages for rich notifications. Functions as an array of
// MessageViews and animates them on transitions. It also supports
// repositioning.
class ASH_EXPORT MessageListView
    : public views::View,
      public views::BoundsAnimatorObserver,
      public message_center::MessageView::SlideObserver {
 public:
  class Observer {
   public:
    virtual void OnAllNotificationsCleared() = 0;
  };

  MessageListView();
  ~MessageListView() override;

  void AddNotificationAt(message_center::MessageView* message_view, int i);
  void RemoveNotification(message_center::MessageView* message_view);
  void UpdateNotification(message_center::MessageView* message_view,
                          const message_center::Notification& notification);
  std::pair<int, message_center::MessageView*> GetNotificationById(
      const std::string& id);
  message_center::MessageView* GetNotificationAt(int index);
  // Return the number of the valid notification. This traverse the items so it
  // costs O(n) time, where n is the number of total notifications.
  size_t GetNotificationCount() const;

  // SetRepositionTarget sets the target that the physical location of
  // the notification at |target_rect| does not change after the repositining.
  // Repositioning is a process to change the positions of the notifications,
  // which is caused by addition/modification/removal of notifications.
  // The term is almost interchangeable with animation.
  void SetRepositionTarget(const gfx::Rect& target_rect);

  void ResetRepositionSession();
  void ClearAllClosableNotifications(const gfx::Rect& visible_scroll_rect);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetBorderPadding();

  // Get the number of notifications above ScrollView's visible rect.
  int GetCountAboveVisibleRect(int y_offset) const;

  // Get the distance from the bottom of ScrollView's visible rect to the bottom
  // of the notification list.
  int GetHeightBelowVisibleRect() const;

  void set_use_fixed_height(bool use_fixed_height) {
    use_fixed_height_ = use_fixed_height;
  }
  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }

  // Overridden from MessageView::SlideObserver
  void OnSlideChanged(const std::string& notification_id) override;

  void UpdateCornerRadius(int index, int top_radius, int bottom_radius);

 protected:
  // Overridden from views::View.
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;

  // Overridden from views::BoundsAnimatorObserver.
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override;
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

 private:
  friend class MessageCenterViewTest;
  friend class MessageListViewTest;

  int GetMarginBetweenItems() const;
  bool IsValidChild(const views::View* child) const;
  void DoUpdateIfPossible();

  // For given notification, expand it if it is allowed to be expanded and is
  // never manually expanded:
  // For other notifications, collapse if it's never manually expanded.
  void ExpandSpecifiedNotificationAndCollapseOthers(
      message_center::MessageView* target_view);

  void ExpandTopNotificationAndCollapseOthers();

  // Animates all notifications to align with the top of the last closed
  // notification.
  void AnimateNotifications();
  // Computes reposition offsets for |AnimateNotificationsAboveTarget|.
  std::vector<int> ComputeRepositionOffsets(const std::vector<int>& heights,
                                            const std::vector<bool>& deleting,
                                            int target_index,
                                            int padding);

  // Schedules animation for a child to the specified position. Returns false
  // if |child| will disappear after the animation.
  bool AnimateChild(views::View* child,  // message_center::SlidableMessageView
                    int top,
                    int height,
                    bool animate_even_on_move);

  // Calculate the new fixed height and update with it. |requested_height|
  // is the minimum height, and actual fixed height should be more than it.
  void UpdateFixedHeight(int requested_height, bool prevent_scroll);

  // Animate clearing one notification.
  void AnimateClearingOneNotification();

  // List of MessageListView::Observer
  base::ObserverList<Observer>::Unchecked observers_;

  // The top position of the reposition target rectangle.
  int reposition_top_;
  int fixed_height_;
  bool has_deferred_task_;
  bool clear_all_started_;
  bool use_fixed_height_;
  bool has_border_padding_;
  std::set<views::View*> adding_views_;
  std::set<views::View*> deleting_views_;
  std::set<views::View*> deleted_when_done_;
  std::list<views::View*> clearing_all_views_;
  views::BoundsAnimator animator_;

  views::ScrollView* scroller_ = nullptr;

  base::WeakPtrFactory<MessageListView> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(MessageListView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_LIST_VIEW_H_
