// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_upgrade_available_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace crostini {

const char kNotifierCrostiniUpgradeAvailable[] = "crostini.upgrade_available";

class CrostiniUpgradeAvailableNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit CrostiniUpgradeAvailableNotificationDelegate(
      Profile* profile,
      base::WeakPtr<CrostiniUpgradeAvailableNotification> notification,
      base::OnceClosure closure)
      : profile_(profile),
        notification_(notification),
        closure_(std::move(closure)) {
    CrostiniManager::GetForProfile(profile_)->UpgradePromptShown(
        DefaultContainerId());
  }

  CrostiniUpgradeAvailableNotificationDelegate(
      const CrostiniUpgradeAvailableNotificationDelegate&) = delete;
  CrostiniUpgradeAvailableNotificationDelegate& operator=(
      const CrostiniUpgradeAvailableNotificationDelegate&) = delete;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    disposition_ =
        CrostiniUpgradeAvailableNotificationClosed::kNotificationBody;
    if (button_index && button_index.value() == 0) {
      disposition_ = CrostiniUpgradeAvailableNotificationClosed::kUpgradeButton;
      HandleButtonClick();
      return;
    }
    HandleBodyClick();
  }

  void HandleButtonClick() {
    ash::CrostiniUpgraderDialog::Show(profile_, base::DoNothing());
    if (notification_) {
      notification_->UpgradeDialogShown();
    }
    Close(false);
  }

  void HandleBodyClick() {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile_, chromeos::settings::mojom::kCrostiniDetailsSubpagePath);
    Close(false);
  }

  void Close(bool by_user) override {
    if (by_user) {
      disposition_ = CrostiniUpgradeAvailableNotificationClosed::kByUser;
    }
    // Run the callback now. The notification might hang around after the
    // closure has been run, so we need to guard it.
    if (closure_) {
      std::move(closure_).Run();
    }
    base::UmaHistogramEnumeration("Crostini.UpgradeAvailable", disposition_);
  }

 private:
  ~CrostiniUpgradeAvailableNotificationDelegate() override = default;

  CrostiniUpgradeAvailableNotificationClosed disposition_ =
      CrostiniUpgradeAvailableNotificationClosed::kUnknown;
  raw_ptr<Profile> profile_;  // Not owned.
  base::WeakPtr<CrostiniUpgradeAvailableNotification> notification_;
  base::OnceClosure closure_;

  base::WeakPtrFactory<CrostiniUpgradeAvailableNotificationDelegate>
      weak_ptr_factory_{this};
};

std::unique_ptr<CrostiniUpgradeAvailableNotification>
CrostiniUpgradeAvailableNotification::Show(Profile* profile,
                                           base::OnceClosure closure) {
  return std::make_unique<CrostiniUpgradeAvailableNotification>(
      profile, std::move(closure));
}

CrostiniUpgradeAvailableNotification::CrostiniUpgradeAvailableNotification(
    Profile* profile,
    base::OnceClosure closure)
    : profile_(profile) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.small_image = gfx::Image(gfx::CreateVectorIcon(
      vector_icons::kFileDownloadIcon, 64, gfx::kGoogleBlue800));
  rich_notification_data.accent_color_id = cros_tokens::kCrosSysPrimary;

  rich_notification_data.buttons.emplace_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_CROSTINI_UPGRADE_AVAILABLE_NOTIFICATION_UPGRADE)));

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_MULTIPLE,
      kNotifierCrostiniUpgradeAvailable,
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_UPGRADE_AVAILABLE_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_UPGRADE_AVAILABLE_NOTIFICATION_BODY),
      ui::ImageModel(), std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kNotifierCrostiniUpgradeAvailable,
          ash::NotificationCatalogName::kCrostiniUpgradeAvailable),
      rich_notification_data,
      base::MakeRefCounted<CrostiniUpgradeAvailableNotificationDelegate>(
          profile_, weak_ptr_factory_.GetWeakPtr(), std::move(closure)));

  notification_->SetSystemPriority();
  ForceRedisplay();
}

CrostiniUpgradeAvailableNotification::~CrostiniUpgradeAvailableNotification() {
  if (notification_ && !profile_->ShutdownStarted()) {
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, notification_->id());
  }
}

void CrostiniUpgradeAvailableNotification::UpgradeDialogShown() {
  notification_->set_buttons({});
  notification_->set_never_timeout(false);
  notification_->set_pinned(false);
  ForceRedisplay();
}

void CrostiniUpgradeAvailableNotification::ForceRedisplay() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

}  // namespace crostini
