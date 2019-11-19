// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tpm_firmware_update_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace tpm_firmware_update {
namespace {

constexpr char kTPMFirmwareUpdateNotificationId[] =
    "chrome://tpm_firmware_update";

class TPMFirmwareUpdateNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit TPMFirmwareUpdateNotificationDelegate(Profile* profile)
      : profile_(profile) {}

 private:
  ~TPMFirmwareUpdateNotificationDelegate() override = default;

  // NotificationDelegate:
  void Close(bool by_user) override {
    if (by_user) {
      profile_->GetPrefs()->SetBoolean(
          prefs::kTPMFirmwareUpdateCleanupDismissed, true);
    }
  }
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    // Show the about page which contains the line item allowing the user to
    // trigger TPM firmware update installation.
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile_, chrome::kHelpSubPage);

    profile_->GetPrefs()->SetBoolean(prefs::kTPMFirmwareUpdateCleanupDismissed,
                                     true);
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, kTPMFirmwareUpdateNotificationId);
  }

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(TPMFirmwareUpdateNotificationDelegate);
};

void OnAvailableUpdateModes(Profile* profile,
                            const std::set<tpm_firmware_update::Mode>& modes) {
  if (modes.count(tpm_firmware_update::Mode::kCleanup) == 0) {
    return;
  }

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kTPMFirmwareUpdateNotificationId,
          l10n_util::GetStringUTF16(IDS_TPM_FIRMWARE_UPDATE_NOTIFICATION_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_TPM_FIRMWARE_UPDATE_NOTIFICATION_MESSAGE,
              ui::GetChromeOSDeviceName()),
          base::string16(), GURL(kTPMFirmwareUpdateNotificationId),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kTPMFirmwareUpdateNotificationId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<TPMFirmwareUpdateNotificationDelegate>(profile),
          gfx::kNoneIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

}  // namespace

void ShowNotificationIfNeeded(Profile* profile) {
  bool cleanup_dismissed = profile->GetPrefs()->GetBoolean(
      prefs::kTPMFirmwareUpdateCleanupDismissed);
  if (cleanup_dismissed) {
    return;
  }

  tpm_firmware_update::GetAvailableUpdateModes(
      base::BindOnce(&OnAvailableUpdateModes, profile), base::TimeDelta());
}

}  // namespace tpm_firmware_update
}  // namespace chromeos
