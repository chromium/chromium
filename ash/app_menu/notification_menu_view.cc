// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_header_view.h"
#include "ash/app_menu/notification_overflow_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_separator.h"

namespace ash {

NotificationMenuView::NotificationMenuView(
    Delegate* notification_item_view_delegate,
    views::SlideOutControllerDelegate* slide_out_controller_delegate,
    const std::string& app_id)
    : app_id_(app_id),
      notification_item_view_delegate_(notification_item_view_delegate),
      slide_out_controller_delegate_(slide_out_controller_delegate),
      double_separator_(AddChildView(std::make_unique<views::MenuSeparator>(
          ui::MenuSeparatorType::DOUBLE_SEPARATOR))),
      header_view_(
          AddChildView(std::make_unique<NotificationMenuHeaderView>())) {
  DCHECK(notification_item_view_delegate_);
  DCHECK(slide_out_controller_delegate_);
  DCHECK(!app_id_.empty())
      << "Only context menus for applications can show notifications.";
}

NotificationMenuView::~NotificationMenuView() = default;

gfx::Size NotificationMenuView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      views::MenuConfig::instance().touchable_menu_min_width,
      double_separator_->GetPreferredSize().height() +
          header_view_->GetPreferredSize({}).height() +
          kNotificationItemViewHeight +
          (overflow_view_ ? overflow_view_->GetPreferredSize().height() : 0));
}

void NotificationMenuView::Layout(PassKey) {
  int y = 0;
  double_separator_->SetBoundsRect(gfx::Rect(
      gfx::Point(0, y),
      gfx::Size(views::MenuConfig::instance().touchable_menu_min_width,
                double_separator_->GetPreferredSize().height())));
  y += double_separator_->GetPreferredSize().height();

  header_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, y), header_view_->GetPreferredSize({})));
  y += header_view_->height();

  auto* item = GetDisplayedNotificationItemView();
  if (item) {
    item->SetBoundsRect(gfx::Rect(gfx::Point(0, y), item->GetPreferredSize()));
    y = item->bounds().bottom();
  }

  if (overflow_view_) {
    overflow_view_->SetBoundsRect(
        gfx::Rect(gfx::Point(0, y), overflow_view_->GetPreferredSize()));
  }
}

bool NotificationMenuView::IsEmpty() const {
  return notification_item_views_.empty();
}

void NotificationMenuView::AddNotificationItemView(
    const message_center::Notification& notification) {
  auto* old_displayed_item = GetDisplayedNotificationItemView();

  auto notification_view = std::make_unique<NotificationItemView>(
      notification_item_view_delegate_, slide_out_controller_delegate_,
      notification.title(), notification.message(), notification.icon(),
      notification.id());
  notification_item_views_.push_front(
      AddChildView(std::move(notification_view)));

  header_view_->UpdateCounter(notification_item_views_.size());

  if (!old_displayed_item)
    return;

  // Push |old_displayed_notification_item_view| to |overflow_view_|.
  old_displayed_item->SetVisible(false);

  const bool overflow_view_created = !overflow_view_;
  if (!overflow_view_)
    overflow_view_ = AddChildView(std::make_unique<NotificationOverflowView>());

  overflow_view_->AddIcon(old_displayed_item->proportional_image_view(),
                          old_displayed_item->notification_id());

  if (overflow_view_created) {
    PreferredSizeChanged();
    // OnOverflowAddedOrRemoved must be called after PreferredSizeChange to
    // ensure that enough room is allocated for the overflow view.
    notification_item_view_delegate_->OnOverflowAddedOrRemoved();
  }
  DeprecatedLayoutImmediately();
}

void NotificationMenuView::UpdateNotificationItemView(
    const message_center::Notification& notification) {
  // Find the view which corresponds to |notification|.
  const auto i = NotificationIterForId(notification.id());
  if (i == notification_item_views_.end())
    return;

  (*i)->UpdateContents(notification.title(), notification.message(),
                       notification.icon());
}

void NotificationMenuView::OnNotificationRemoved(
    const std::string& notification_id) {
  // Find the view which corresponds to |notification_id|.
  const auto i = NotificationIterForId(notification_id);
  if (i == notification_item_views_.end())
    return;
  const bool removed_displayed_notification =
      *i == GetDisplayedNotificationItemView();

  RemoveChildViewT(*i);
  notification_item_views_.erase(i);
  header_view_->UpdateCounter(notification_item_views_.size());

  if (removed_displayed_notification) {
    // Display the next notification.
    auto* item = GetDisplayedNotificationItemView();
    if (item) {
      item->SetVisible(true);
      if (overflow_view_)
        overflow_view_->RemoveIcon(item->notification_id());
    }
  } else if (overflow_view_) {
    overflow_view_->RemoveIcon(notification_id);
  }

  if (overflow_view_ && overflow_view_->is_empty()) {
    // Remove and delete |overflow_view_|.
    RemoveChildViewT(overflow_view_.get());
    overflow_view_ = nullptr;
    PreferredSizeChanged();
    notification_item_view_delegate_->OnOverflowAddedOrRemoved();
  }
}

ui::Layer* NotificationMenuView::GetSlideOutLayer() {
  auto* item = GetDisplayedNotificationItemView();
  return item ? item->layer() : nullptr;
}

const NotificationItemView*
NotificationMenuView::GetDisplayedNotificationItemView() const {
  return notification_item_views_.empty() ? nullptr
                                          : notification_item_views_.front();
}

const std::string& NotificationMenuView::GetDisplayedNotificationID() const {
  DCHECK(!notification_item_views_.empty());
  return GetDisplayedNotificationItemView()->notification_id();
}

NotificationMenuView::NotificationItemViews::iterator
NotificationMenuView::NotificationIterForId(const std::string& id) {
  return base::ranges::find(notification_item_views_, id,
                            &NotificationItemView::notification_id);
}

BEGIN_METADATA(NotificationMenuView)
END_METADATA

}  // namespace ash
