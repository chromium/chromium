// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eche_app/eche_app_notification_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"

namespace chromeos {
namespace eche_app {

const char kEcheAppScreenLockNotifierId[] =
    "eche_app_notification_ids.screen_lock";

// TODO(crbug.com/1241352): This should probably have a ?p=<FEATURE_NAME> at
// some point.
const char kEcheAppLearnMoreUrl[] = "https://support.google.com/chromebook";

namespace {

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const gfx::Image& icon,
    const message_center::RichNotificationData& rich_notification_data,
    message_center::NotificationDelegate* delegate) {
  return std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, std::u16string() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id),
      rich_notification_data, delegate);
}

}  // namespace

EcheAppNotificationController::EcheAppNotificationController(Profile* profile)
    : profile_(profile) {}

EcheAppNotificationController::~EcheAppNotificationController() {}
void EcheAppNotificationController::LaunchSettings() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2);
}

void EcheAppNotificationController::LaunchLearnMore() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(kEcheAppLearnMoreUrl),
      /* from_user_interaction= */ true);
}

void EcheAppNotificationController::ShowScreenLockNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_SETTINGS_BUTTON)));
  rich_notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_LEARN_MORE)));

  ShowNotification(CreateNotification(
      kEcheAppScreenLockNotifierId,
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_MESSAGE),
      gfx::Image(), rich_notification_data,
      new NotificationDelegate(kEcheAppScreenLockNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EcheAppNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

EcheAppNotificationController::NotificationDelegate::NotificationDelegate(
    const std::string& notification_id,
    const base::WeakPtr<EcheAppNotificationController>& notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

EcheAppNotificationController::NotificationDelegate::~NotificationDelegate() {}
void EcheAppNotificationController::NotificationDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (!button_index)
    return;

  if (*button_index == 0) {
    notification_controller_->LaunchSettings();
  } else {
    DCHECK_EQ(1, *button_index);
    notification_controller_->LaunchLearnMore();
  }
}

}  // namespace eche_app
}  // namespace chromeos
