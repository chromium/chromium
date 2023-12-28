// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/lock_screen_notification_controller.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "base/observer_list_types.h"
#include "components/session_manager/session_manager_types.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

LockScreenNotificationController::LockScreenNotificationController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

LockScreenNotificationController::~LockScreenNotificationController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
const char LockScreenNotificationController::kLockScreenNotificationId[] =
    "lock_screen";

std::unique_ptr<message_center::Notification>
LockScreenNotificationController::CreateNotification() {
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  optional_fields.priority = message_center::NotificationPriority::MIN_PRIORITY;
  return ash::CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kLockScreenNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_LOCKSCREEN_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_LOCKSCREEN_NOTIFICATION_DESCRIPTION),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kLockScreenNotifierId,
                                 NotificationCatalogName::kLockScreen),
      optional_fields,
      /*delegate=*/nullptr, vector_icons::kLockIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

void LockScreenNotificationController::OnNotificationAdded(
    const std::string& id) {
  auto* message_center = message_center::MessageCenter::Get();
  if (message_center->FindNotificationById(kLockScreenNotificationId)) {
    return;
  }

  if (message_center_utils::AreNotificationsHiddenOnLockscreen()) {
    message_center->AddNotification(CreateNotification());
  }
}

void LockScreenNotificationController::OnNotificationRemoved(
    const std::string& id,
    bool by_user) {
  if (message_center_utils::AreNotificationsHiddenOnLockscreen()) {
    return;
  }

  auto* message_center = message_center::MessageCenter::Get();
  if (message_center->FindNotificationById(kLockScreenNotificationId)) {
    message_center->RemoveNotification(kLockScreenNotificationId,
                                       /*by_user=*/false);
  }
}

void LockScreenNotificationController::OnSessionStateChanged(
    session_manager::SessionState state) {
  is_screen_locked_ = state == session_manager::SessionState::LOCKED;

  auto* message_center = message_center::MessageCenter::Get();

  // Observe the `MessageCenter` for notification changes while the screen is
  // locked.
  if (is_screen_locked_) {
    message_center_observation_.Observe(message_center);
  } else {
    message_center_observation_.Reset();
  }

  if (is_screen_locked_ &&
      message_center_utils::AreNotificationsHiddenOnLockscreen()) {
    message_center->AddNotification(CreateNotification());
    return;
  }

  if (!is_screen_locked_ &&
      message_center->FindNotificationById(kLockScreenNotificationId)) {
    message_center->RemoveNotification(kLockScreenNotificationId,
                                       /*by_user=*/false);
  }
}

}  // namespace ash
