// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_utils.h"

#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace message_center_utils {

bool CompareNotifications(message_center::Notification* n1,
                          message_center::Notification* n2) {
  if (n1->pinned() && !n2->pinned())
    return true;
  if (!n1->pinned() && n2->pinned())
    return false;
  return message_center::CompareTimestampSerial()(n1, n2);
}

std::vector<message_center::Notification*> GetSortedVisibleNotifications() {
  auto visible_notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  std::vector<message_center::Notification*> sorted_notifications;
  std::copy(visible_notifications.begin(), visible_notifications.end(),
            std::back_inserter(sorted_notifications));
  std::sort(sorted_notifications.begin(), sorted_notifications.end(),
            CompareNotifications);
  return sorted_notifications;
}

size_t GetNotificationCount() {
  size_t count = 0;
  for (message_center::Notification* notification :
       message_center::MessageCenter::Get()->GetVisibleNotifications()) {
    const std::string& notifier = notification->notifier_id().id;
    // Don't count these notifications since we have `CameraMicTrayItemView` to
    // show indicators on the systray.
    if (notifier == kVmCameraMicNotifierId)
      continue;

    ++count;
  }
  return count;
}

}  // namespace message_center_utils

}  // namespace ash
