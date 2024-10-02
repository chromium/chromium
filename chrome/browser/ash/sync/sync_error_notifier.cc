// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_error_notifier.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/account_id/account_id.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/trusted_vault/features.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {
namespace {

const char kProfileSyncNotificationId[] = "chrome://settings/sync/";

struct BubbleViewParameters {
  int title_id;
  int message_id;
  base::RepeatingClosure click_action;
};

void ShowSyncSetup(Profile* profile) {
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile);
  if (login_ui->current_login_ui()) {
    // TODO(michaelpg): The LoginUI might be on an inactive desktop.
    // See crbug.com/354280.
    login_ui->current_login_ui()->FocusUI();
    return;
  }

  if (crosapi::browser_util::IsLacrosEnabled()) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, chromeos::settings::mojom::kSyncSetupSubpagePath);
  } else {
    // TODO(crbug.com/40210838): remove this once it's not possible to use ash
    // as a primary browser.
    chrome::ShowSettingsSubPageForProfile(profile, chrome::kSyncSetupSubPage);
  }
}

void TriggerSyncKeyRetrieval(Profile* profile) {
  if (!crosapi::browser_util::IsAshWebBrowserEnabled() &&
      base::FeatureList::IsEnabled(
          trusted_vault::kChromeOSTrustedVaultUseWebUIDialog)) {
    OpenDialogForSyncKeyRetrieval(
        profile, syncer::TrustedVaultUserActionTriggerForUMA::kNotification);
  } else {
    // TODO(crbug.com/40264837): clean up once not reachable.
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    OpenTabForSyncKeyRetrieval(
        displayer.browser(),
        syncer::TrustedVaultUserActionTriggerForUMA::kNotification);
  }
}

void TriggerSyncRecoverabilityDegradedFix(Profile* profile) {
  if (!crosapi::browser_util::IsAshWebBrowserEnabled() &&
      base::FeatureList::IsEnabled(
          trusted_vault::kChromeOSTrustedVaultUseWebUIDialog)) {
    OpenDialogForSyncKeyRecoverabilityDegraded(
        profile, syncer::TrustedVaultUserActionTriggerForUMA::kNotification);
  } else {
    // TODO(crbug.com/40264837): clean up once not reachable.
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    OpenTabForSyncKeyRecoverabilityDegraded(
        displayer.browser(),
        syncer::TrustedVaultUserActionTriggerForUMA::kNotification);
  }
}

BubbleViewParameters GetBubbleViewParameters(
    Profile* profile,
    syncer::SyncService* sync_service) {
  if (ShouldShowSyncPassphraseError(sync_service)) {
    BubbleViewParameters params;
    params.title_id = IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE;
    params.message_id = IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE;
    // |profile| is guaranteed to outlive the callback because the ownership of
    // the notification gets transferred to NotificationDisplayService, which is
    // a keyed service that cannot outlive the profile.
    params.click_action =
        base::BindRepeating(&ShowSyncSetup, base::Unretained(profile));
    return params;
  }

  if (sync_service->GetUserSettings()
          ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
    BubbleViewParameters params;
    params.title_id =
        sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
            ? IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE
            : IDS_SYNC_ERROR_PASSWORDS_BUBBLE_VIEW_TITLE;
    params.message_id =
        sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
            ? IDS_SYNC_NEEDS_KEYS_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE
            : IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE;

    params.click_action = base::BindRepeating(&TriggerSyncKeyRetrieval,
                                              base::Unretained(profile));
    return params;
  }

  DCHECK(
      sync_service->GetUserSettings()->IsTrustedVaultRecoverabilityDegraded());

  BubbleViewParameters params;
  params.title_id = IDS_SYNC_NEEDS_VERIFICATION_BUBBLE_VIEW_TITLE;
  params.message_id =
      sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
          ? IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE
          : IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE;

  params.click_action = base::BindRepeating(
      &TriggerSyncRecoverabilityDegradedFix, base::Unretained(profile));
  return params;
}

}  // namespace

SyncErrorNotifier::SyncErrorNotifier(syncer::SyncService* sync_service,
                                     Profile* profile)
    : sync_service_(sync_service), profile_(profile) {
  // Create a unique notification ID for this profile.
  notification_id_ =
      kProfileSyncNotificationId + profile_->GetProfileUserName();

  sync_service_->AddObserver(this);
  OnStateChanged(sync_service_);
}

SyncErrorNotifier::~SyncErrorNotifier() {
  DCHECK(!sync_service_) << "SyncErrorNotifier::Shutdown() was not called";
}

void SyncErrorNotifier::Shutdown() {
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SyncErrorNotifier::OnStateChanged(syncer::SyncService* service) {
  DCHECK_EQ(service, sync_service_);

  const bool should_display_notification =
      ShouldShowSyncPassphraseError(sync_service_) ||
      sync_service_->GetUserSettings()
          ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ||
      sync_service_->GetUserSettings()->IsTrustedVaultRecoverabilityDegraded();

  if (should_display_notification == notification_displayed_) {
    return;
  }

  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  if (!should_display_notification) {
    notification_displayed_ = false;
    display_service->Close(NotificationHandler::Type::TRANSIENT,
                           notification_id_);
    return;
  }

  // Error state just got triggered. There shouldn't be previous notification.
  // Let's display one.
  DCHECK(!notification_displayed_ && should_display_notification);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kProfileSyncNotificationId, ash::NotificationCatalogName::kSyncError);

  // Set |profile_id| for multi-user notification blocker.
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  BubbleViewParameters parameters =
      GetBubbleViewParameters(profile_, sync_service_);

  // Add a new notification.
  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
      l10n_util::GetStringUTF16(parameters.title_id),
      l10n_util::GetStringUTF16(parameters.message_id), std::u16string(),
      GURL(notification_id_), notifier_id,
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          parameters.click_action),
      vector_icons::kNotificationWarningIcon,
      message_center::SystemNotificationWarningLevel::WARNING);

  display_service->Display(NotificationHandler::Type::TRANSIENT, notification,
                           /*metadata=*/nullptr);
  notification_displayed_ = true;
}

}  // namespace ash
