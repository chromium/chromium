// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_notification_blocker.h"

#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

WelcomeTourNotificationBlocker::WelcomeTourNotificationBlocker()
    : message_center::NotificationBlocker(
          message_center::MessageCenter::Get()) {}

WelcomeTourNotificationBlocker::~WelcomeTourNotificationBlocker() {
  // Hide all popups just before blocking ends so that the user is not bombarded
  // at the end of the tour. Note that system priority notifications are
  // excluded so the user does not miss any time critical notifications.
  auto popups = message_center()->GetPopupNotificationsWithoutBlocker(*this);
  for (auto* popup : popups) {
    if (popup->priority() == message_center::SYSTEM_PRIORITY) {
      continue;
    }
    message_center()->MarkSinglePopupAsShown(
        popup->id(),
        /*mark_notification_as_read=*/false);
  }
}

bool WelcomeTourNotificationBlocker::ShouldShowNotification(
    const message_center::Notification& notification) const {
  return false;
}

bool WelcomeTourNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  return false;
}

}  // namespace ash
