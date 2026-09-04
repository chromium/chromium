// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_error_notifier.h"

#include "ash/constants/chrome_webui_url_constants.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/user_manager/user.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/message_center.h"
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

bool IsNewSignInNonSyncingUser(const syncer::SyncService* sync_service) {
  return sync_service && !sync_service->HasSyncConsent() &&
         syncer::IsReplaceSyncPromosWithSignInPromosEnabled();
}

bool ShouldShowSyncDisabledViaDashboardError(
    const syncer::SyncService* sync_service) {
  return sync_service &&
         sync_service->GetUserSettings()->IsSyncFeatureDisabledViaDashboard() &&
         IsNewSignInNonSyncingUser(sync_service);
}

void OpenOSSyncSettings(Profile* profile) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kSyncControlsSubpagePath);
}

void OpenSyncSettings(Profile* profile) {
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile);
  if (login_ui->current_login_ui()) {
    // TODO(michaelpg): The LoginUI might be on an inactive desktop.
    // See crbug.com/41095891.
    login_ui->current_login_ui()->FocusUI();
    return;
  }

  chrome::ShowSettingsSubPageForProfile(
      profile, SyncErrorNotifier::GetDestinationSubpage(
                   SyncServiceFactory::GetForProfile(profile)));
}

void TriggerSyncKeyRetrieval(Profile* profile) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  OpenTabForSyncKeyRetrieval(
      displayer.browser_window_interface(),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kNotification);
}

void TriggerSyncRecoverabilityDegradedFix(Profile* profile) {
  // TODO(crbug.com/40264837): clean up once not reachable.
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  OpenTabForSyncKeyRecoverabilityDegraded(
      displayer.browser_window_interface(),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kNotification);
}

BubbleViewParameters GetBubbleViewParameters(
    Profile* profile,
    syncer::SyncService* sync_service) {
  if (ShouldShowSyncDisabledViaDashboardError(sync_service)) {
    BubbleViewParameters params;
    params.title_id = IDS_SYNC_DASHBOARD_DISABLED_BUBBLE_VIEW_TITLE;
    params.message_id = IDS_SYNC_DASHBOARD_DISABLED_BUBBLE_VIEW_MESSAGE;
    params.click_action =
        base::BindRepeating(&OpenOSSyncSettings, base::Unretained(profile));
    return params;
  }

  if (ShouldShowSyncPassphraseError(sync_service)) {
    BubbleViewParameters params;
    params.title_id = IsNewSignInNonSyncingUser(sync_service)
                          ? IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE_2
                          : IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE;
    params.message_id = IsNewSignInNonSyncingUser(sync_service)
                            ? IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE_2
                            : IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE;
    // |profile| outlives the click callback since notifications are tied to the
    // active user session and MessageCenter is torn down before profiles during
    // shutdown.
    params.click_action =
        base::BindRepeating(&OpenSyncSettings, base::Unretained(profile));
    return params;
  }

  if (sync_service->GetUserSettings()
          ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
    BubbleViewParameters params;
    params.title_id =
        IsNewSignInNonSyncingUser(sync_service)
            ? IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE_2
            : (sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
                   ? IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE
                   : IDS_SYNC_ERROR_PASSWORDS_BUBBLE_VIEW_TITLE);
    params.message_id =
        sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
            ? (IsNewSignInNonSyncingUser(sync_service)
                   ? IDS_SYNC_NEEDS_KEYS_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE_2
                   : IDS_SYNC_NEEDS_KEYS_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE)
            : (IsNewSignInNonSyncingUser(sync_service)
                   ? IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE_2
                   : IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE);

    params.click_action = base::BindRepeating(&TriggerSyncKeyRetrieval,
                                              base::Unretained(profile));
    return params;
  }

  DCHECK(
      sync_service->GetUserSettings()->IsTrustedVaultRecoverabilityDegraded());

  BubbleViewParameters params;
  params.title_id = IsNewSignInNonSyncingUser(sync_service)
                        ? IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE_2
                        : IDS_SYNC_NEEDS_VERIFICATION_BUBBLE_VIEW_TITLE;
  params.message_id =
      sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
          ? (IsNewSignInNonSyncingUser(sync_service)
                 ? IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE_2
                 : IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE)
          : (IsNewSignInNonSyncingUser(sync_service)
                 ? IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE_2
                 : IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE);

  params.click_action = base::BindRepeating(
      &TriggerSyncRecoverabilityDegradedFix, base::Unretained(profile));
  return params;
}

}  // namespace

// static
std::string SyncErrorNotifier::GetDestinationSubpage(
    syncer::SyncService* sync_service) {
  return IsNewSignInNonSyncingUser(sync_service)
             ? ash::chrome_urls::kAccountSubPage
             : ash::chrome_urls::kSyncSetupSubPage;
}

SyncErrorNotifier::SyncErrorNotifier(syncer::SyncService* sync_service,
                                     Profile* profile)
    : sync_service_(sync_service), profile_(profile) {
  // Create a unique user-scoped notification ID for this profile.
  const user_manager::User& user = CHECK_DEREF(
      BrowserContextHelper::Get()->GetUserByBrowserContext(profile_));
  notification_id_ = CreateUserScopedNotificationId(kProfileSyncNotificationId,
                                                    user.username_hash());

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
      ShouldShowSyncDisabledViaDashboardError(sync_service_) ||
      ShouldShowSyncPassphraseError(sync_service_) ||
      sync_service_->GetUserSettings()
          ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ||
      sync_service_->GetUserSettings()->IsTrustedVaultRecoverabilityDegraded();

  if (should_display_notification == notification_displayed_) {
    return;
  }

  if (!should_display_notification) {
    notification_displayed_ = false;
    message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                             /*by_user=*/false);
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
      CHECK_DEREF(
          BrowserContextHelper::Get()->GetUserByBrowserContext(profile_))
          .GetAccountId()
          .GetUserEmail();

  BubbleViewParameters parameters =
      GetBubbleViewParameters(profile_, sync_service_);

  // Add a new notification.
  message_center::MessageCenter::Get()->AddNotification(
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
          l10n_util::GetStringUTF16(parameters.title_id),
          l10n_util::GetStringUTF16(parameters.message_id), std::u16string(),
          /*origin_url=*/GURL(), notifier_id,
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              parameters.click_action),
          ::features::IsRoundedIconsEnabled()
              ? vector_icons::kInfoFilledIcon
              : vector_icons::kNotificationWarningOldIcon,
          message_center::SystemNotificationWarningLevel::WARNING));
  notification_displayed_ = true;
}

void SyncErrorNotifier::OnSyncShutdown(syncer::SyncService*) {
  // Unreachable, since this service is Shutdown() before the SyncService.
  NOTREACHED();
}

}  // namespace ash
