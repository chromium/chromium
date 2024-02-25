// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCALABLE_IPH_SCALABLE_IPH_ASH_NOTIFICATION_VIEW_H_
#define ASH_SCALABLE_IPH_SCALABLE_IPH_ASH_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/notification_center/views/ash_notification_view.h"

namespace message_center {
class MessageView;
class Notification;
class NotificationHeaderView;
}  // namespace message_center

namespace ash {

// A customized notification view for scalable IPH that adds the summary text.
class ASH_EXPORT ScalableIphAshNotificationView : public AshNotificationView {
  METADATA_HEADER(ScalableIphAshNotificationView, AshNotificationView)

 public:
  ScalableIphAshNotificationView(
      const message_center::Notification& notification,
      bool shown_in_popup);
  ScalableIphAshNotificationView(const ScalableIphAshNotificationView&) =
      delete;
  ScalableIphAshNotificationView& operator=(
      const ScalableIphAshNotificationView&) = delete;
  ~ScalableIphAshNotificationView() override;

  // Creates a notification with a custom summary text.
  static std::unique_ptr<message_center::MessageView> CreateView(
      const message_center::Notification& notification,
      bool shown_in_popup);

  // AshNotificationView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;

  message_center::NotificationHeaderView* GetHeaderRowForTesting();

 private:
  friend class ScalableIphAshNotificationViewTest;
};

}  // namespace ash

#endif  // ASH_SCALABLE_IPH_SCALABLE_IPH_ASH_NOTIFICATION_VIEW_H_
