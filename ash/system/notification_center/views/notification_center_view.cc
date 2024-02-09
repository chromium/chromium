// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_center_view.h"

#include <climits>
#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
#include "ash/system/notification_center/views/message_center_scroll_bar.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Inset the top and the bottom of the scroll bar so it won't be clipped by
// rounded corners.
constexpr auto kScrollBarInsets = gfx::Insets::TLBR(16, 0, 16, 0);

}  // namespace

// TODO(https://b/307940238): Use `unique_ptrs` instead of `new`.
NotificationCenterView::NotificationCenterView()
    : notification_bar_(new StackedNotificationBar(this)),
      // TODO(crbug.com/1247455): Determine how to use ScrollWithLayers without
      // breaking ARC.
      scroller_(new views::ScrollView()),
      notification_list_view_(new NotificationListView(this)) {
  notification_list_view_tracker_.SetView(notification_list_view_);
  notification_list_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      [](raw_ptr<NotificationListView>& notification_list_view) {
        notification_list_view = nullptr;
      },
      std::ref(notification_list_view_)));

  auto* scroll_bar = new MessageCenterScrollBar();
  scroll_bar->SetInsets(kScrollBarInsets);
  scroll_bar_ = scroll_bar;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kMessageCenterPadding)));
}

NotificationCenterView::~NotificationCenterView() {
  scroller_->RemoveObserver(this);
}

void NotificationCenterView::Init() {
  CHECK(!features::IsNotificationCenterControllerEnabled());
  notification_list_view_->Init();
  AddChildViews();
}

void NotificationCenterView::Init(
    const std::vector<message_center::Notification*>& notifications) {
  CHECK(features::IsNotificationCenterControllerEnabled() &&
        !features::AreOngoingProcessesEnabled());
  notification_list_view_->Init(notifications);
  AddChildViews();
}

void NotificationCenterView::Init(
    const std::vector<message_center::Notification*>& unpinned_notifications,
    std::unique_ptr<views::View> pinned_notification_list_view) {
  CHECK(features::AreOngoingProcessesEnabled());
  notification_list_view_->Init(unpinned_notifications);

  AddChildViews(std::move(pinned_notification_list_view));
}

void NotificationCenterView::AddChildViews(
    std::unique_ptr<views::View> pinned_notification_list_view) {
  // TODO(crbug.com/1247455): Be able to do
  // SetContentsLayerType(LAYER_NOT_DRAWN).
  auto scroller_contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(kMessageCenterPadding)
          .Build();
  if (features::AreOngoingProcessesEnabled()) {
    scroller_contents_view->AddChildView(
        std::move(pinned_notification_list_view));
  }
  scroller_contents_view->AddChildView(notification_list_view_);
  scroller_->SetContents(std::move(scroller_contents_view));
  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetBackgroundColor(std::nullopt);
  scroller_->SetVerticalScrollBar(base::WrapUnique(scroll_bar_.get()));
  scroller_->SetDrawOverflowIndicator(false);
  scroller_->SetPaintToLayer();
  scroller_->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{
      static_cast<float>(kMessageCenterScrollViewCornerRadius)});

  AddChildView(scroller_.get());

  // Make sure the scroll view takes up the entirety of available height in the
  // notification center view. We're relying on a max height constraint
  // for the `TrayBubbleView` so we need to set flex for the scroll view here.
  scroller_->AddObserver(this);
  scroller_->ClipHeightTo(0, INT_MAX);
  layout_manager_->SetFlexForView(scroller_, 1);

  on_contents_scrolled_subscription_ =
      scroller_->AddContentsScrolledCallback(base::BindRepeating(
          &NotificationCenterView::OnContentsScrolled, base::Unretained(this)));

  AddChildView(notification_bar_.get());
}

bool NotificationCenterView::UpdateNotificationBar() {
  if (!notification_list_view_) {
    return false;
  }

  return notification_bar_->Update(
      notification_list_view_->GetTotalNotificationCount(),
      notification_list_view_->GetTotalPinnedNotificationCount(),
      GetStackedNotifications());
}

void NotificationCenterView::ClearAllNotifications() {
  if (!notification_list_view_) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_StackingBarClearAll"));

  notification_list_view_->ClearAllWithAnimation();
}

bool NotificationCenterView::IsScrollBarVisible() const {
  return scroll_bar_->GetVisible();
}

void NotificationCenterView::OnNotificationSlidOut() {
  UpdateNotificationBar();
}

void NotificationCenterView::OnNotificationAdded(const std::string& id) {
  CHECK(features::IsNotificationCenterControllerEnabled());
  if (!notification_list_view_) {
    return;
  }

  notification_list_view_->OnNotificationAdded(id);
}

void NotificationCenterView::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  CHECK(features::IsNotificationCenterControllerEnabled());
  if (!notification_list_view_) {
    return;
  }

  notification_list_view_->OnNotificationRemoved(id, by_user);
}

void NotificationCenterView::OnNotificationUpdated(const std::string& id) {
  CHECK(features::IsNotificationCenterControllerEnabled());
  if (!notification_list_view_) {
    return;
  }

  notification_list_view_->OnNotificationUpdated(id);
}

void NotificationCenterView::ListPreferredSizeChanged() {
  PreferredSizeChanged();

  if (GetWidget() && !GetWidget()->IsClosed()) {
    GetWidget()->SynthesizeMouseMoveEvent();
  }
}

void NotificationCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void NotificationCenterView::OnViewBoundsChanged(views::View* observed_view) {
  UpdateNotificationBar();
}

void NotificationCenterView::OnContentsScrolled() {
  UpdateNotificationBar();
}

std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
NotificationCenterView::GetStackedNotifications() const {
  if (!notification_list_view_) {
    return std::vector<
        raw_ptr<message_center::Notification, VectorExperimental>>{};
  }

  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty()) {
    scroller_->SetBoundsRect(GetContentsBounds());
  }

  const int y_offset = scroller_->GetVisibleRect().bottom() - scroller_->y();
  return notification_list_view_->GetNotificationsBelowY(y_offset);
}

std::vector<std::string>
NotificationCenterView::GetNonVisibleNotificationIdsInViewHierarchy() const {
  if (!notification_list_view_) {
    return std::vector<std::string>{};
  }

  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty()) {
    scroller_->SetBoundsRect(GetContentsBounds());
  }

  const int y_offset_above = scroller_->GetVisibleRect().y() - scroller_->y() +
                             kStackedNotificationBarHeight;
  auto above_id_list =
      notification_list_view_->GetNotificationIdsAboveY(y_offset_above);
  const int y_offset_below =
      scroller_->GetVisibleRect().bottom() - scroller_->y();
  const auto below_id_list =
      notification_list_view_->GetNotificationIdsBelowY(y_offset_below);
  above_id_list.insert(above_id_list.end(), below_id_list.begin(),
                       below_id_list.end());
  return above_id_list;
}

BEGIN_METADATA(NotificationCenterView)
END_METADATA

}  // namespace ash
