// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_VIEW_CONTAINER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_VIEW_CONTAINER_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class NotificationListView;
class NotificationSwipeControlView;

// Container view for `MessageView` objects, which are initialized with a
// `message_center::Notification`.
class MessageViewContainer : public views::View,
                             public message_center::MessageView::Observer {
  METADATA_HEADER(MessageViewContainer, views::View)

 public:
  explicit MessageViewContainer(
      std::unique_ptr<message_center::MessageView> message_view,
      NotificationListView* list_view = nullptr);
  MessageViewContainer(const MessageViewContainer&) = delete;
  MessageViewContainer& operator=(const MessageViewContainer&) = delete;
  ~MessageViewContainer() override = default;

  // Calls `GetHeightForWidth` on the cached `message_view_`.
  int CalculateHeight() const;

  // Updates the corner radius based on if the view is at the top or the bottom
  // of its parent list view. If `force_update` is true, the corner radius and
  // background will be updated even if `is_top` and `is_bottom` have the same
  // value as the stored variables.
  void UpdateBorder(const bool is_top,
                    const bool is_bottom,
                    const bool force_update = false);

  // Gets the `notification_id` stored in `message_view_`.
  const std::string GetNotificationId() const;

  // Forwards call to `UpdateWithNotification` in `message_view_`.
  void UpdateWithNotification(const message_center::Notification& notification);

  message_center::MessageView* message_view() { return message_view_; }
  const message_center::MessageView* message_view() const {
    return message_view_;
  }

  base::TimeDelta GetBoundsAnimationDuration() const;

  void SetExpandedBySystem(bool expanded);

  void SlideOutAndClose();

  void CloseSwipeControl();

  // Allows NotificationListView to force preferred size to change during
  // animations.
  void TriggerPreferredSizeChangedForAnimation();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // MessageView::Observer:
  void OnSlideChanged(const std::string& notification_id) override;
  void OnSlideEnded(const std::string& notification_id) override;
  void OnPreSlideOut(const std::string& notification_id) override;
  void OnSlideOut(const std::string& notification_id) override;

  void set_start_bounds(const gfx::Rect& start_bounds) {
    start_bounds_ = start_bounds;
  }

  void set_target_bounds(const gfx::Rect& target_bounds) {
    target_bounds_ = target_bounds;
  }

  void set_is_removed(bool is_removed) { is_removed_ = is_removed; }

  void set_needs_bounds_animation(bool needs_bounds_animation) {
    needs_bounds_animation_ = needs_bounds_animation;
  }

  void set_disable_default_background(bool disable_default_background) {
    disable_default_background_ = disable_default_background;
  }

  void set_need_update_corner_radius(bool need_update_corner_radius) {
    need_update_corner_radius_ = need_update_corner_radius;
  }

  gfx::Rect start_bounds() const { return start_bounds_; }
  gfx::Rect target_bounds() const { return target_bounds_; }
  bool is_removed() const { return is_removed_; }
  bool needs_bounds_animation() const { return needs_bounds_animation_; }
  bool is_slid_out() { return is_slid_out_; }

  // Returns if the notification is pinned i.e. can be removed manually.
  bool IsPinned() const;

  // Returns if the notification is a parent of other grouped notifications.
  bool IsGroupParent() const;

  // Returns the direction that the notification is swiped out. If swiped to the
  // left, it returns -1 and if sipwed to the right, it returns 1. By default
  // (i.e. the notification is removed but not by touch gesture), it returns 1.
  int GetSlideDirection() const {
    return message_view_->GetSlideAmount() < 0 ? -1 : 1;
  }

  bool is_top() { return is_top_; }
  bool is_bottom() { return is_bottom_; }

 private:
  // Used to track if this view is at the top or bottom of its parent list view
  // and prevent unnecessary updates.
  bool is_top_ = false;
  bool is_bottom_ = false;

  // Cached to return to previous state after slide animation ends.
  bool previous_is_bottom_ = false;
  bool previous_is_top_ = false;

  // The bounds that the container starts animating from. If not animating, it's
  // ignored.
  gfx::Rect start_bounds_;

  // The final bounds of the container. If not animating, it's same as the
  // actual bounds().
  gfx::Rect target_bounds_;

  // True when the notification is removed and during slide out animation.
  bool is_removed_ = false;

  // True if the notification is slid out completely.
  bool is_slid_out_ = false;

  // True if the notification is slid out through SlideOutAndClose()
  // programagically. False if slid out manually by the user.
  bool is_slid_out_programatically_ = false;

  // Whether expanded state is being set programmatically. Used to prevent
  // animating programmatic expands which occur on open.
  bool expanding_by_system_ = false;

  // Set to flag the view as requiring an expand or collapse animation.
  bool needs_bounds_animation_ = false;

  // `need_update_corner_radius_` indicates that we need to update the corner
  // radius of the view when sliding.
  bool need_update_corner_radius_ = true;

  // Indicates if this view should not draw a background, used for custom
  // notification views which handle their own background.
  bool disable_default_background_ = false;

  // Owned by the views hierarchy.
  const raw_ptr<NotificationListView> list_view_;
  raw_ptr<NotificationSwipeControlView> control_view_;
  raw_ptr<message_center::MessageView> message_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_VIEW_CONTAINER_H_
