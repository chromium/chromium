// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scalable_iph/scalable_iph_ash_notification_view.h"

#include "base/check.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/views/notification_header_view.h"

namespace ash {

ScalableIphAshNotificationView::ScalableIphAshNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup)
    : AshNotificationView(notification, shown_in_popup) {
  UpdateWithNotification(notification);
}

ScalableIphAshNotificationView::~ScalableIphAshNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
ScalableIphAshNotificationView::CreateView(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<ScalableIphAshNotificationView>(notification,
                                                          shown_in_popup);
}

void ScalableIphAshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  NotificationViewBase::UpdateWithNotification(notification);

  auto* header = header_row();
  CHECK(header);
  header->SetSummaryText(scalable_iph::kNotificationSummaryText);
}

message_center::NotificationHeaderView*
ScalableIphAshNotificationView::GetHeaderRowForTesting() {
  return header_row();
}

BEGIN_METADATA(ScalableIphAshNotificationView)
END_METADATA

}  // namespace ash
