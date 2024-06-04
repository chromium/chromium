// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/message_view_container.h"

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/notification_center/views/notification_swipe_control_view.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr int kNotificationOuterCornerRadius =
    kMessageCenterScrollViewCornerRadius;
constexpr int kNotificationInnerCornerRadius =
    kMessageCenterNotificationInnerCornerRadius;

}  // namespace

MessageViewContainer::MessageViewContainer(
    std::unique_ptr<message_center::MessageView> message_view,
    NotificationListView* list_view)
    : list_view_(list_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  control_view_ = AddChildView(
      std::make_unique<NotificationSwipeControlView>(message_view.get()));

  // Load `MessageView` expand state.
  message_center::ExpandState expand_state =
      message_center::MessageCenter::Get()->GetNotificationExpandState(
          message_view->notification_id());
  if (expand_state != message_center::ExpandState::DEFAULT) {
    message_view->SetExpanded(expand_state ==
                              message_center::ExpandState::USER_EXPANDED);
  }

  message_view_ = AddChildView(std::move(message_view));
  message_view_->AddObserver(this);
}

int MessageViewContainer::CalculateHeight() const {
  return message_view_ ? message_view_->GetHeightForWidth(
                             GetNotificationInMessageCenterWidth())
                       : 0;
}

void MessageViewContainer::UpdateBorder(const bool is_top,
                                        const bool is_bottom,
                                        const bool force_update) {
  if (is_top_ == is_top && is_bottom_ == is_bottom && !force_update) {
    return;
  }

  is_top_ = is_top;
  is_bottom_ = is_bottom;

  int top_radius =
      is_top ? kNotificationOuterCornerRadius : kNotificationInnerCornerRadius;
  int bottom_radius = is_bottom ? kNotificationOuterCornerRadius
                                : kNotificationInnerCornerRadius;

  message_view_->UpdateCornerRadius(top_radius, bottom_radius);

  // Custom notifications handle their background separately.
  if (disable_default_background_) {
    return;
  }

  message_view_->SetBackground(
      notification_style_utils::CreateNotificationBackground(
          top_radius, bottom_radius, /*is_popup_notification=*/false,
          /*is_grouped_child_notification=*/false));
}

const std::string MessageViewContainer::GetNotificationId() const {
  return message_view_->notification_id();
}

void MessageViewContainer::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_view_->UpdateWithNotification(notification);
}

base::TimeDelta MessageViewContainer::GetBoundsAnimationDuration() const {
  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          message_view()->notification_id());
  if (!notification) {
    return base::Milliseconds(0);
  }
  return message_view()->GetBoundsAnimationDuration(*notification);
}

void MessageViewContainer::SetExpandedBySystem(bool expanded) {
  base::AutoReset<bool> scoped_reset(&expanding_by_system_, true);
  message_view_->SetExpanded(expanded);
}

void MessageViewContainer::SlideOutAndClose() {
  is_slid_out_programatically_ = true;
  message_view_->SlideOutAndClose(/*direction=*/1);
}

void MessageViewContainer::CloseSwipeControl() {
  message_view_->CloseSwipeControl();
}

void MessageViewContainer::TriggerPreferredSizeChangedForAnimation() {
  views::View::PreferredSizeChanged();
}

gfx::Size MessageViewContainer::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (list_view_ && list_view_->IsAnimatingExpandOrCollapseContainer(this)) {
    // Width should never change, only height.
    return gfx::Size(GetNotificationInMessageCenterWidth(),
                     gfx::Tween::IntValueBetween(
                         list_view_->GetCurrentAnimationValue(),
                         start_bounds_.height(), target_bounds_.height()));
  }
  return gfx::Size(GetNotificationInMessageCenterWidth(), CalculateHeight());
}

void MessageViewContainer::ChildPreferredSizeChanged(views::View* child) {
  // If we've already been removed, ignore new child size changes.
  if (is_removed_) {
    return;
  }

  // PreferredSizeChanged will trigger
  // NotificationListView::ChildPreferredSizeChanged.
  absl::Cleanup defer_preferred_size_changed = [this] {
    PreferredSizeChanged();
  };

  // Ignore non-user triggered expand/collapses.
  if (expanding_by_system_) {
    return;
  }

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          message_view()->notification_id());
  if (!notification) {
    return;
  }

  needs_bounds_animation_ = true;
}

void MessageViewContainer::OnSlideChanged(const std::string& notification_id) {
  control_view_->UpdateButtonsVisibility();

  if (notification_id != GetNotificationId() || !list_view_ ||
      message_view_->GetSlideAmount() == 0 || !need_update_corner_radius_) {
    return;
  }

  need_update_corner_radius_ = false;
  previous_is_top_ = is_top_;
  previous_is_bottom_ = is_bottom_;
  UpdateBorder(/*is_top=*/true, /*is_bottom=*/true);
  size_t index = list_view_->GetIndexOf(this).value();

  // Also update the corner radius for the views above and below when sliding.
  auto list_child_views = list_view_->children();

  auto* above_view =
      (index == 0)
          ? nullptr
          : static_cast<MessageViewContainer*>(list_child_views[index - 1]);
  auto* below_view =
      (index == list_child_views.size() - 1)
          ? nullptr
          : static_cast<MessageViewContainer*>(list_child_views[index + 1]);

  if (above_view) {
    above_view->UpdateBorder(above_view->is_top(),
                             /*is_bottom=*/true);
  }
  if (below_view) {
    below_view->UpdateBorder(/*is_top=*/true, below_view->is_bottom());
  }
}

void MessageViewContainer::OnSlideEnded(const std::string& notification_id) {
  if (notification_id != GetNotificationId() || !list_view_) {
    return;
  }

  std::optional<size_t> index = list_view_->GetIndexOf(this);
  if (!index.has_value()) {
    return;
  }

  // Also update the corner radius for the views above and below when sliding.
  auto list_child_views = list_view_->children();
  auto* above_view = (index == size_t{0})
                         ? nullptr
                         : static_cast<MessageViewContainer*>(
                               list_child_views[index.value() - 1]);
  auto* below_view = (index == list_child_views.size() - 1)
                         ? nullptr
                         : static_cast<MessageViewContainer*>(
                               list_child_views[index.value() + 1]);

  // Reset the corner radius of views to their normal state.
  UpdateBorder(previous_is_top_, previous_is_bottom_);
  set_need_update_corner_radius(true);

  if (above_view && !above_view->is_slid_out()) {
    above_view->UpdateBorder(above_view->is_top(), /*is_bottom=*/false);
    above_view->set_need_update_corner_radius(true);
  }

  if (below_view && !below_view->is_slid_out()) {
    below_view->UpdateBorder(/*is_top=*/false, below_view->is_bottom());
    below_view->set_need_update_corner_radius(true);
  }
}

void MessageViewContainer::OnPreSlideOut(const std::string& notification_id) {
  if (!is_slid_out_programatically_) {
    metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/true,
                                   /*is_popup=*/false);
  }
}

void MessageViewContainer::OnSlideOut(const std::string& notification_id) {
  is_slid_out_ = true;
  set_is_removed(true);
  if (list_view_) {
    list_view_->OnNotificationSlidOut();
  }
}

bool MessageViewContainer::IsPinned() const {
  return message_view_->GetMode() == message_center::MessageView::Mode::PINNED;
}

bool MessageViewContainer::IsGroupParent() const {
  return message_center::MessageCenter::Get()
      ->FindNotificationById(GetNotificationId())
      ->group_parent();
}

BEGIN_METADATA(MessageViewContainer);
END_METADATA

}  // namespace ash
