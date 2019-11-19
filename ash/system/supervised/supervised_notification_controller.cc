// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/supervised_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {
const char kNotifierSupervisedUser[] = "ash.locally-managed-user";
}  // namespace

const char SupervisedNotificationController::kNotificationId[] =
    "chrome://user/locally-managed";

SupervisedNotificationController::SupervisedNotificationController() = default;

SupervisedNotificationController::~SupervisedNotificationController() = default;

void SupervisedNotificationController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  OnUserSessionUpdated(account_id);
}

void SupervisedNotificationController::OnUserSessionAdded(
    const AccountId& account_id) {
  OnUserSessionUpdated(account_id);
}

void SupervisedNotificationController::OnUserSessionUpdated(
    const AccountId& account_id) {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  if (!session_controller->IsUserSupervised())
    return;

  // Get the active user session.
  DCHECK(session_controller->IsActiveUserSessionStarted());
  const UserSession* const user_session = session_controller->GetUserSession(0);
  DCHECK(user_session);

  // Only respond to updates for the active user.
  if (user_session->user_info.account_id != account_id)
    return;

  // Show notifications when custodian data first becomes available on login
  // and if the custodian data changes.
  if (custodian_email_ == user_session->custodian_email &&
      second_custodian_email_ == user_session->second_custodian_email) {
    return;
  }
  custodian_email_ = user_session->custodian_email;
  second_custodian_email_ = user_session->second_custodian_email;

  CreateOrUpdateNotification();
}

// static
void SupervisedNotificationController::CreateOrUpdateNotification() {
  // No notification for the child user.
  if (Shell::Get()->session_controller()->IsUserChild())
    return;

  // Regular supervised user.
  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL),
      GetSupervisedUserMessage(), base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierSupervisedUser),
      message_center::RichNotificationData(), nullptr,
      kNotificationSupervisedUserIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  // AddNotification does an update if the notification already exists.
  MessageCenter::Get()->AddNotification(std::move(notification));
}

}  // namespace ash
