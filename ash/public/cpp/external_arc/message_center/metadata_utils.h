// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METADATA_UTILS_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METADATA_UTILS_H_

#include "ash/components/arc/mojom/notifications.mojom.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

std::unique_ptr<message_center::Notification>
CreateNotificationFromArcNotificationData(
    const message_center::NotificationType notification_type,
    const std::string& notification_id,
    arc::mojom::ArcNotificationData* data,
    const message_center::NotifierId notifier_id,
    message_center::RichNotificationData rich_data,
    scoped_refptr<message_center::NotificationDelegate> delegate);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METADATA_UTILS_H_
