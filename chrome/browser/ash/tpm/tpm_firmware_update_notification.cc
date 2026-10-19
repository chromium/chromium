// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/tpm/tpm_firmware_update_notification.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace tpm_firmware_update {
namespace {

constexpr char kTPMFirmwareUpdateNotificationId[] =
    "chrome://tpm_firmware_update";

class TPMFirmwareUpdateNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  TPMFirmwareUpdateNotificationDelegate(
      content::BrowserContext* browser_context,
      std::string notification_id)
      : browser_context_(browser_context),
        notification_id_(std::move(notification_id)) {}

  TPMFirmwareUpdateNotificationDelegate(
      const TPMFirmwareUpdateNotificationDelegate&) = delete;
  TPMFirmwareUpdateNotificationDelegate& operator=(
      const TPMFirmwareUpdateNotificationDelegate&) = delete;

 private:
  ~TPMFirmwareUpdateNotificationDelegate() override = default;

  // NotificationDelegate:
  void Close(bool by_user) override {
    if (by_user) {
      user_prefs::UserPrefs::Get(browser_context_)
          ->SetBoolean(ash::prefs::kTPMFirmwareUpdateCleanupDismissed, true);
    }
  }
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    // Show the about page which contains the line item allowing the user to
    // trigger TPM firmware update installation.
    const user_manager::User& user =
        CHECK_DEREF(ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
            browser_context_));
    ash::SettingsAppManager::Get()->Open(
        user,
        {.sub_page = chromeos::settings::mojom::kAboutChromeOsSectionPath});

    user_prefs::UserPrefs::Get(browser_context_)
        ->SetBoolean(ash::prefs::kTPMFirmwareUpdateCleanupDismissed, true);
    message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                             /*by_user=*/false);
  }

  const raw_ptr<content::BrowserContext> browser_context_;
  const std::string notification_id_;
};

void OnAvailableUpdateModes(content::BrowserContext* browser_context,
                            const std::set<tpm_firmware_update::Mode>& modes) {
  if (modes.count(tpm_firmware_update::Mode::kCleanup) == 0) {
    return;
  }

  const user_manager::User& user =
      CHECK_DEREF(ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          browser_context));
  const std::string notification_id = CreateUserScopedNotificationId(
      kTPMFirmwareUpdateNotificationId, user.username_hash());
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kTPMFirmwareUpdateNotificationId,
      NotificationCatalogName::kTPMFirmwareUpdate);
  notifier_id.profile_id = user.GetAccountId().GetUserEmail();

  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_TPM_FIRMWARE_UPDATE_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(IDS_TPM_FIRMWARE_UPDATE_NOTIFICATION_MESSAGE,
                                 ui::GetChromeOSDeviceName()),
      std::u16string(), notifier_id, message_center::RichNotificationData(),
      base::MakeRefCounted<TPMFirmwareUpdateNotificationDelegate>(
          browser_context, notification_id),
      gfx::VectorIcon::EmptyIcon(),
      message_center::SystemNotificationWarningLevel::WARNING);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace

void ShowNotificationIfNeeded(content::BrowserContext* browser_context) {
  bool cleanup_dismissed =
      user_prefs::UserPrefs::Get(browser_context)
          ->GetBoolean(ash::prefs::kTPMFirmwareUpdateCleanupDismissed);
  if (cleanup_dismissed) {
    return;
  }

  tpm_firmware_update::GetAvailableUpdateModes(
      base::BindOnce(&OnAvailableUpdateModes, browser_context),
      base::TimeDelta());
}

}  // namespace tpm_firmware_update
}  // namespace ash
