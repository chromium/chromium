// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/notification_interaction_handler_impl.h"
#include "ash/components/phonehub/notification.h"
#include "base/logging.h"

namespace ash {
namespace phonehub {

NotificationInteractionHandlerImpl::NotificationInteractionHandlerImpl() {}

NotificationInteractionHandlerImpl::~NotificationInteractionHandlerImpl() =
    default;

void NotificationInteractionHandlerImpl::HandleNotificationClicked(
    int64_t notification_id,
    const Notification::AppMetadata& app_metadata) {
  NotifyNotificationClicked(notification_id, app_metadata);
}

}  // namespace phonehub
}  // namespace ash
