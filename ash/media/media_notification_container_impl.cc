// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_container_impl.h"

#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/native_theme/native_theme_dark_aura.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Width/height of the dismiss button.
constexpr int kDismissButtonIconSideLength = 12;

// Width/height of the colored-background container of the dismiss button.
constexpr int kControlButtonsContainerSideLength =
    2 * kDismissButtonIconSideLength;

}  // anonymous namespace

MediaNotificationContainerImpl::MediaNotificationContainerImpl(
    const message_center::Notification& notification,
    base::WeakPtr<media_message_center::MediaSessionNotificationItem> item)
    : message_center::MessageView(notification) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto control_buttons_container = std::make_unique<views::View>();

  // We paint to a layer here so that we can modify opacity of that layer to
  // match the opacity of the |control_buttons_view_| layer.
  control_buttons_container->SetPaintToLayer();
  control_buttons_container->layer()->SetFillsBoundsOpaquely(false);

  // Vertically center the dismiss icon within the container.
  constexpr int top_margin =
      (kControlButtonsContainerSideLength - kDismissButtonIconSideLength) / 2;
  control_buttons_container->SetProperty(views::kMarginsKey,
                                         gfx::Insets(top_margin, 0, 0, 0));

  control_buttons_container->SetPreferredSize(gfx::Size(
      kControlButtonsContainerSideLength, kControlButtonsContainerSideLength));
  control_buttons_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  control_buttons_container_ = control_buttons_container.get();

  auto control_buttons_view =
      std::make_unique<message_center::NotificationControlButtonsView>(this);
  control_buttons_view->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
  control_buttons_view_ =
      control_buttons_container_->AddChildView(std::move(control_buttons_view));

  auto view = std::make_unique<media_message_center::MediaNotificationView>(
      this, std::move(item), std::move(control_buttons_container),
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      message_center::kNotificationWidth,
      /*should_show_icon=*/true);
  view_ = AddChildView(std::move(view));

  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
}

MediaNotificationContainerImpl::~MediaNotificationContainerImpl() = default;

void MediaNotificationContainerImpl::UpdateWithNotification(
    const message_center::Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  UpdateControlButtonsVisibilityWithNotification(notification);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

message_center::NotificationControlButtonsView*
MediaNotificationContainerImpl::GetControlButtonsView() const {
  return control_buttons_view_;
}

void MediaNotificationContainerImpl::SetExpanded(bool expanded) {
  view_->SetExpanded(expanded);
}

void MediaNotificationContainerImpl::UpdateCornerRadius(int top_radius,
                                                        int bottom_radius) {
  MessageView::SetCornerRadius(top_radius, bottom_radius);
  view_->UpdateCornerRadius(top_radius, bottom_radius);
}

void MediaNotificationContainerImpl::UpdateControlButtonsVisibility() {
  message_center::MessageView::UpdateControlButtonsVisibility();

  // The above call may update the opacity of the control buttons to 0 or 1. We
  // need to update the container layer opacity to match.
  control_buttons_container_->layer()->SetOpacity(
      control_buttons_view_->layer()->opacity());
}

void MediaNotificationContainerImpl::OnExpanded(bool expanded) {
  PreferredSizeChanged();
}

void MediaNotificationContainerImpl::OnColorsChanged(SkColor foreground,
                                                     SkColor background) {
  // We need to update the foreground and background colors of the dismiss icon
  // to ensure proper contrast against the artwork.
  control_buttons_view_->SetButtonIconColors(foreground);
  control_buttons_container_->SetBackground(views::CreateRoundedRectBackground(
      background, kControlButtonsContainerSideLength / 2));
}

void MediaNotificationContainerImpl::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdateControlButtonsVisibility();
      break;
    default:
      break;
  }

  View::OnMouseEvent(event);
}

void MediaNotificationContainerImpl::
    UpdateControlButtonsVisibilityWithNotification(
        const message_center::Notification& notification) {
  // Media notifications do not use the settings and snooze buttons.
  DCHECK(!notification.should_show_settings_button());
  DCHECK(!notification.should_show_snooze_button());

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  UpdateControlButtonsVisibility();
}

}  // namespace ash
