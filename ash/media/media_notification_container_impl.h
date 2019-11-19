// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "components/media_message_center/media_notification_container.h"
#include "ui/message_center/views/message_view.h"

namespace media_message_center {
class MediaSessionNotificationItem;
class MediaNotificationView;
}  // namespace media_message_center

namespace ash {

// MediaNotificationContainerImpl will show up as a custom notification. It will
// show the currently playing media and provide playback controls. There will
// also be control buttons (e.g. close) in the top right corner that will hide
// and show if the notification is hovered.
class ASH_EXPORT MediaNotificationContainerImpl
    : public message_center::MessageView,
      public media_message_center::MediaNotificationContainer {
 public:
  explicit MediaNotificationContainerImpl(
      const message_center::Notification& notification,
      base::WeakPtr<media_message_center::MediaSessionNotificationItem> item);
  ~MediaNotificationContainerImpl() override;

  // message_center::MessageView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;
  void SetExpanded(bool expanded) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void UpdateControlButtonsVisibility() override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override {}
  void OnMediaSessionMetadataChanged() override {}
  void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void OnMediaArtworkChanged(const gfx::ImageSkia& image) override {}
  void OnColorsChanged(SkColor foreground, SkColor background) override;
  void OnHeaderClicked() override {}

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  // Contains |control_buttons_view_| and puts a circular colored background
  // behind it to ensure proper contrast.
  views::View* control_buttons_container_ = nullptr;

  // View containing close and settings buttons.
  message_center::NotificationControlButtonsView* control_buttons_view_ =
      nullptr;

  media_message_center::MediaNotificationView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImpl);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
