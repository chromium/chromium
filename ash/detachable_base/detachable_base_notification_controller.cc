// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/detachable_base/detachable_base_notification_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

constexpr char kDetachableBaseNotifierId[] = "ash.system.detachable_base";

}  // namespace

const char DetachableBaseNotificationController::kBaseChangedNotificationId[] =
    "chrome://settings/detachable_base/detachable_base_changed";

const char
    DetachableBaseNotificationController::kBaseRequiresUpdateNotificationId[] =
        "chrome://settings/detachable_base/detachable_base_requires_update";

DetachableBaseNotificationController::DetachableBaseNotificationController(
    DetachableBaseHandler* detachable_base_handler)
    : detachable_base_handler_(detachable_base_handler) {
  detachable_base_observation_.Observe(detachable_base_handler);
  ShowPairingNotificationIfNeeded();
}

DetachableBaseNotificationController::~DetachableBaseNotificationController() =
    default;

void DetachableBaseNotificationController::OnDetachableBasePairingStatusChanged(
    DetachableBasePairingStatus status) {
  ShowPairingNotificationIfNeeded();
}

void DetachableBaseNotificationController::
    OnDetachableBaseRequiresUpdateChanged(bool requires_update) {
  if (!requires_update) {
    RemoveUpdateRequiredNotification();
    return;
  }

  std::u16string title = l10n_util::GetStringUTF16(
      IDS_ASH_DETACHABLE_BASE_NOTIFICATION_UPDATE_NEEDED_TITLE);
  std::u16string message = l10n_util::GetStringUTF16(
      IDS_ASH_DETACHABLE_BASE_NOTIFICATION_UPDATE_NEEDED_MESSAGE);

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kBaseRequiresUpdateNotificationId, title, message, std::u16string(),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kDetachableBaseNotifierId,
              NotificationCatalogName::kDetachableBaseRequiresUpdate),
          message_center::RichNotificationData(), nullptr,
          vector_icons::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  // Set system priority so the notification gets shown when the user session is
  // blocked.
  notification->SetSystemPriority();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void DetachableBaseNotificationController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // Remove notification shown for the provious user.
  RemovePairingNotification();

  ShowPairingNotificationIfNeeded();
}

void DetachableBaseNotificationController::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Remove the existing notification if the session gets blocked - lock UI
  // displays its own warning for base changes, when needed.
  RemovePairingNotification();

  ShowPairingNotificationIfNeeded();
}

void DetachableBaseNotificationController::ShowPairingNotificationIfNeeded() {
  // Do not show the notification if the session is blocked - login/lock UI have
  // their own UI for notifying the user of the detachable base change.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return;

  const UserSession* active_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  if (!active_session)
    return;

  DetachableBasePairingStatus pairing_status =
      detachable_base_handler_->GetPairingStatus();
  if (pairing_status == DetachableBasePairingStatus::kNone)
    return;

  const UserInfo& user_info = active_session->user_info;
  if (pairing_status == DetachableBasePairingStatus::kAuthenticated &&
      detachable_base_handler_->PairedBaseMatchesLastUsedByUser(user_info)) {
    // Set the current base as last used by the user.
    // PairedBaseMatchesLastUsedByUser returns true if the user has not
    // previously used a base, so make sure the last used base value is actually
    // set.
    detachable_base_handler_->SetPairedBaseAsLastUsedByUser(user_info);
    return;
  }

  // Remove any previously added notifications to ensure the new notification is
  // shown to the user as a pop-up.
  RemovePairingNotification();

  message_center::RichNotificationData options;
  options.never_timeout = true;
  options.priority = message_center::MAX_PRIORITY;

  std::u16string title = l10n_util::GetStringUTF16(
      IDS_ASH_DETACHABLE_BASE_NOTIFICATION_DEVICE_CHANGED_TITLE);
  std::u16string message = l10n_util::GetStringUTF16(
      IDS_ASH_DETACHABLE_BASE_NOTIFICATION_DEVICE_CHANGED_MESSAGE);

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kBaseChangedNotificationId,
          title, message, std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kDetachableBaseNotifierId,
              NotificationCatalogName::kDetachableBasePairingNotification),
          options, nullptr, vector_icons::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

  // At this point the session is unblocked - mark the current base as used by
  // user (as they have just been notified about the base change).
  if (pairing_status == DetachableBasePairingStatus::kAuthenticated)
    detachable_base_handler_->SetPairedBaseAsLastUsedByUser(user_info);
}

void DetachableBaseNotificationController::RemovePairingNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kBaseChangedNotificationId, false);
}

void DetachableBaseNotificationController::RemoveUpdateRequiredNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kBaseRequiresUpdateNotificationId, false);
}

}  // namespace ash
