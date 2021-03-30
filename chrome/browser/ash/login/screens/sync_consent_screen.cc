// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/sync_consent_screen.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace {

// Delay showing chrome sync settings by this amount of time to make them
// show on top of the restored tabs and windows.
constexpr base::TimeDelta kSyncConsentSettingsShowDelay =
    base::TimeDelta::FromSeconds(3);

syncer::SyncService* GetSyncService(Profile* profile) {
  if (ProfileSyncServiceFactory::HasSyncService(profile))
    return ProfileSyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

void RecordUmaReviewFollowingSetup(bool value) {
  base::UmaHistogramBoolean("OOBE.SyncConsentScreen.ReviewFollowingSetup",
                            value);
}

}  // namespace

// static
std::string SyncConsentScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

// static
void SyncConsentScreen::MaybeLaunchSyncConsentSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(
          ::prefs::kShowSyncSettingsOnSessionStart)) {
    // TODO (alemate): In a very special case when chrome is exiting at the very
    // moment we show Settings, it might crash here because profile could be
    // already destroyed. This needs to be fixed.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](Profile* profile) {
              profile->GetPrefs()->ClearPref(
                  ::prefs::kShowSyncSettingsOnSessionStart);
              chrome::ShowSettingsSubPageForProfile(profile,
                                                    chrome::kSyncSetupSubPage);
            },
            base::Unretained(profile)),
        kSyncConsentSettingsShowDelay);
  }
}

SyncConsentScreen::SyncConsentScreen(SyncConsentScreenView* view,
                                     const ScreenExitCallback& exit_callback)
    : BaseScreen(SyncConsentScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

SyncConsentScreen::~SyncConsentScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void SyncConsentScreen::Init() {
  if (is_initialized_)
    return;
  is_initialized_ = true;
  user_ = user_manager::UserManager::Get()->GetPrimaryUser();
  profile_ = ProfileHelper::Get()->GetProfileByUser(user_);
  UpdateScreen();
}

void SyncConsentScreen::Finish(Result result) {
  DCHECK(profile_);
  // Always set completed, even if the dialog was skipped (e.g. by policy).
  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kSyncOobeCompleted, true);
  // Record whether the dialog was shown, skipped, etc.
  base::UmaHistogramEnumeration("OOBE.SyncConsentScreen.Behavior", behavior_);
  // Record the final state of the sync service.
  syncer::SyncService* service = GetSyncService(profile_);
  bool sync_enabled = service && service->CanSyncFeatureStart() &&
                      service->GetUserSettings()->IsSyncEverythingEnabled();
  base::UmaHistogramBoolean("OOBE.SyncConsentScreen.SyncEnabled", sync_enabled);
  exit_callback_.Run(result);
}

bool SyncConsentScreen::MaybeSkip(WizardContext* context) {
  Init();

  switch (behavior_) {
    case SyncScreenBehavior::kUnknown:
    case SyncScreenBehavior::kShow:
      return false;
    case SyncScreenBehavior::kSkipNonGaiaAccount:
    case SyncScreenBehavior::kSkipPublicAccount:
    case SyncScreenBehavior::kSkipPermissionsPolicy:
    case SyncScreenBehavior::kSkipAndEnableNonBrandedBuild:
    case SyncScreenBehavior::kSkipAndEnableEmphemeralUser:
    case SyncScreenBehavior::kSkipAndEnableScreenPolicy:
      MaybeEnableSyncForSkip();
      Finish(Result::NOT_APPLICABLE);
      return true;
  }
}

void SyncConsentScreen::ShowImpl() {
  Init();

  if (behavior_ != SyncScreenBehavior::kShow) {
    // Wait for updates and set the loading throbber to be visible.
    view_->SetThrobberVisible(true /*visible*/);
    syncer::SyncService* service = GetSyncService(profile_);
    if (service)
      sync_service_observer_.Add(service);
  }
  // Show the entire screen.
  // If SyncScreenBehavior is show, this should show the sync consent screen.
  // If SyncScreenBehavior is unknown, this should show the loading throbber.
  view_->Show();
}

void SyncConsentScreen::HideImpl() {
  sync_service_observer_.RemoveAll();
  view_->Hide();
}

void SyncConsentScreen::OnStateChanged(syncer::SyncService* sync) {
  UpdateScreen();
}

void SyncConsentScreen::OnContinueAndReview(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  if (is_hidden())
    return;
  RecordUmaReviewFollowingSetup(true);
  RecordConsent(CONSENT_GIVEN, consent_description, consent_confirmation);
  profile_->GetPrefs()->SetBoolean(::prefs::kShowSyncSettingsOnSessionStart,
                                   true);
  Finish(Result::NEXT);
}

void SyncConsentScreen::OnContinueWithDefaults(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  if (is_hidden())
    return;
  RecordUmaReviewFollowingSetup(false);
  RecordConsent(CONSENT_GIVEN, consent_description, consent_confirmation);
  Finish(Result::NEXT);
}

void SyncConsentScreen::OnContinue(
    const std::vector<int>& consent_description,
    int consent_confirmation,
    SyncConsentScreenHandler::UserChoice choice) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  if (is_hidden())
    return;
  base::UmaHistogramEnumeration("OOBE.SyncConsentScreen.UserChoice", choice);
  // Record that the user saw the consent text, regardless of which features
  // they chose to enable.
  RecordConsent(CONSENT_GIVEN, consent_description, consent_confirmation);
  bool enable_sync = choice == SyncConsentScreenHandler::UserChoice::kAccepted;
  UpdateSyncSettings(enable_sync);
  Finish(Result::NEXT);
}

void SyncConsentScreen::UpdateSyncSettings(bool enable_sync) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  // For historical reasons, Chrome OS always has a "sync-consented" primary
  // account in IdentityManager and always has browser sync "enabled". If the
  // user disables the browser sync toggle we disable all browser data types,
  // as if the user had opened browser sync settings and turned off all the
  // toggles.
  // TODO(crbug.com/1046746, crbug.com/1050677): Once all Chrome OS code is
  // converted to the "consent aware" IdentityManager API, and the browser sync
  // settings WebUI is converted to allow browser sync to be turned on/off, then
  // this workaround can be removed.
  syncer::SyncService* sync_service = GetSyncService(profile_);
  if (sync_service) {
    syncer::SyncUserSettings* sync_settings = sync_service->GetUserSettings();
    sync_settings->SetOsSyncFeatureEnabled(enable_sync);
    if (!enable_sync) {
      syncer::UserSelectableTypeSet empty_set;
      sync_settings->SetSelectedTypes(/*sync_everything=*/false, empty_set);
    }
    sync_settings->SetSyncRequested(true);
    sync_settings->SetFirstSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  }
  // Set a "sync-consented" primary account. See comment above.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  DCHECK(!account_id.empty());
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(account_id);

  // Only enable URL-keyed metrics if the user turned on browser sync.
  if (enable_sync) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile_);
    if (consent_service)
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  }
}

void SyncConsentScreen::MaybeEnableSyncForSkip() {
  // Prior to SplitSettingsSync, sync is autostarted during ProfileSyncService
  // creation, so sync is already in the right state.
  if (!chromeos::features::IsSplitSettingsSyncEnabled())
    return;

  switch (behavior_) {
    case SyncScreenBehavior::kUnknown:
    case SyncScreenBehavior::kShow:
      NOTREACHED();
      return;
    case SyncScreenBehavior::kSkipNonGaiaAccount:
    case SyncScreenBehavior::kSkipPublicAccount:
    case SyncScreenBehavior::kSkipPermissionsPolicy:
      // Nothing to do.
      return;
    case SyncScreenBehavior::kSkipAndEnableNonBrandedBuild:
    case SyncScreenBehavior::kSkipAndEnableEmphemeralUser:
    case SyncScreenBehavior::kSkipAndEnableScreenPolicy:
      UpdateSyncSettings(/*enable_sync=*/true);
      return;
  }
}

void SyncConsentScreen::SetDelegateForTesting(
    SyncConsentScreen::SyncConsentScreenTestDelegate* delegate) {
  test_delegate_ = delegate;
}

SyncConsentScreen::SyncConsentScreenTestDelegate*
SyncConsentScreen::GetDelegateForTesting() const {
  return test_delegate_;
}

SyncConsentScreen::SyncScreenBehavior SyncConsentScreen::GetSyncScreenBehavior()
    const {
  // Skip for users without Gaia account (e.g. Active Directory).
  if (!user_->HasGaiaAccount())
    return SyncScreenBehavior::kSkipNonGaiaAccount;

  // Skip for public user.
  if (user_->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return SyncScreenBehavior::kSkipPublicAccount;

  // Skip for non-branded (e.g. developer) builds. Check this after the account
  // type checks so we don't try to enable sync in browser_tests for those
  // account types.
  if (!WizardController::IsBrandedBuild())
    return SyncScreenBehavior::kSkipAndEnableNonBrandedBuild;

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  // Skip for non-regular ephemeral users.
  if (user_manager->IsUserNonCryptohomeDataEphemeral(user_->GetAccountId()) &&
      (user_->GetType() != user_manager::USER_TYPE_REGULAR)) {
    return SyncScreenBehavior::kSkipAndEnableEmphemeralUser;
  }

  // Skip if the sync consent screen is disabled by policy, for example, in
  // education scenarios. https://crbug.com/841156
  if (!profile_->GetPrefs()->GetBoolean(::prefs::kEnableSyncConsent))
    return SyncScreenBehavior::kSkipAndEnableScreenPolicy;

  // Skip if sync-the-feature is disabled by policy.
  if (IsProfileSyncDisabledByPolicy())
    return SyncScreenBehavior::kSkipPermissionsPolicy;

  if (IsProfileSyncEngineInitialized())
    return SyncScreenBehavior::kShow;

  return SyncScreenBehavior::kUnknown;
}

void SyncConsentScreen::UpdateScreen() {
  const SyncScreenBehavior new_behavior = GetSyncScreenBehavior();
  if (new_behavior == SyncScreenBehavior::kUnknown)
    return;

  const SyncScreenBehavior old_behavior = behavior_;
  behavior_ = new_behavior;

  if (is_hidden() || behavior_ == old_behavior)
    return;

  if (behavior_ == SyncScreenBehavior::kShow) {
    view_->SetThrobberVisible(false /*visible*/);
    GetSyncService(profile_)->RemoveObserver(this);
  } else {
    MaybeEnableSyncForSkip();
    Finish(Result::NEXT);
  }
}

void SyncConsentScreen::RecordConsent(
    ConsentGiven consent_given,
    const std::vector<int>& consent_description,
    int consent_confirmation) {
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile_);
  // The user might not consent to browser sync, so use the "unconsented" ID.
  const CoreAccountId& google_account_id =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  // TODO(alemate): Support unified_consent_enabled
  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(consent_confirmation);
  for (int id : consent_description) {
    sync_consent.add_description_grd_ids(id);
  }
  sync_consent.set_status(consent_given == CONSENT_GIVEN
                              ? sync_pb::UserConsentTypes::GIVEN
                              : sync_pb::UserConsentTypes::NOT_GIVEN);
  consent_auditor->RecordSyncConsent(google_account_id, sync_consent);

  if (test_delegate_) {
    test_delegate_->OnConsentRecordedIds(consent_given, consent_description,
                                         consent_confirmation);
  }
}

bool SyncConsentScreen::IsProfileSyncDisabledByPolicy() const {
  if (test_sync_disabled_by_policy_.has_value())
    return test_sync_disabled_by_policy_.value();
  const syncer::SyncService* sync_service = GetSyncService(profile_);
  return sync_service->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

bool SyncConsentScreen::IsProfileSyncEngineInitialized() const {
  if (test_sync_engine_initialized_.has_value())
    return test_sync_engine_initialized_.value();
  const syncer::SyncService* sync_service = GetSyncService(profile_);
  return sync_service->IsEngineInitialized();
}

void SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(bool value) {
  test_sync_disabled_by_policy_ = value;
}
void SyncConsentScreen::SetProfileSyncEngineInitializedForTesting(bool value) {
  test_sync_engine_initialized_ = value;
}

}  // namespace chromeos
