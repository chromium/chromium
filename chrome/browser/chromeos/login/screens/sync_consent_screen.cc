// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/pref_names.h"
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

}  // namespace

// static
void SyncConsentScreen::MaybeLaunchSyncConsentSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kShowSyncSettingsOnSessionStart)) {
    // TODO (alemate): In a very special case when chrome is exiting at the very
    // moment we show Settings, it might crash here because profile could be
    // already destroyed. This needs to be fixed.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](Profile* profile) {
              profile->GetPrefs()->ClearPref(
                  prefs::kShowSyncSettingsOnSessionStart);
              chrome::ShowSettingsSubPageForProfile(profile,
                                                    chrome::kSyncSetupSubPage);
            },
            base::Unretained(profile)),
        kSyncConsentSettingsShowDelay);
  }
}

SyncConsentScreen::SyncConsentScreen(
    SyncConsentScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(SyncConsentScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

SyncConsentScreen::~SyncConsentScreen() {
  view_->Bind(NULL);
}

void SyncConsentScreen::Show() {
  user_ = user_manager::UserManager::Get()->GetPrimaryUser();
  profile_ = ProfileHelper::Get()->GetProfileByUser(user_);

  UpdateScreen();

  if (behavior_ == SyncScreenBehavior::SKIP) {
    exit_callback_.Run();
    return;
  }

  shown_ = true;
  if (behavior_ != SyncScreenBehavior::SHOW) {
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

void SyncConsentScreen::Hide() {
  shown_ = false;
  sync_service_observer_.RemoveAll();
  view_->Hide();
}

void SyncConsentScreen::OnStateChanged(syncer::SyncService* sync) {
  UpdateScreen();
}

void SyncConsentScreen::OnContinueAndReview(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  RecordConsent(CONSENT_GIVEN, consent_description, consent_confirmation);
  profile_->GetPrefs()->SetBoolean(prefs::kShowSyncSettingsOnSessionStart,
                                   true);
  exit_callback_.Run();
}

void SyncConsentScreen::OnContinueWithDefaults(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  RecordConsent(CONSENT_GIVEN, consent_description, consent_confirmation);
  exit_callback_.Run();
}

void SyncConsentScreen::OnAcceptAndContinue(
    const std::vector<int>& consent_description,
    int consent_confirmation,
    bool enable_os_sync) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  // The user only consented to the feature if they left the toggle on.
  RecordConsent(enable_os_sync ? CONSENT_GIVEN : CONSENT_NOT_GIVEN,
                consent_description, consent_confirmation);
  profile_->GetPrefs()->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled,
                                   enable_os_sync);
  exit_callback_.Run();
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
  // Skip for users without Gaia account.
  if (!user_->HasGaiaAccount())
    return SyncScreenBehavior::SKIP;

  // Skip for public user.
  if (user_->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return SyncScreenBehavior::SKIP;

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  // Skip for non-regular ephemeral users.
  if (user_manager->IsUserNonCryptohomeDataEphemeral(user_->GetAccountId()) &&
      (user_->GetType() != user_manager::USER_TYPE_REGULAR)) {
    return SyncScreenBehavior::SKIP;
  }

  // Skip if disabled by policy.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kEnableSyncConsent)) {
    return SyncScreenBehavior::SKIP;
  }

  // Skip for sync-disabled case.
  if (IsProfileSyncDisabledByPolicy())
    return SyncScreenBehavior::SKIP;

  if (IsProfileSyncEngineInitialized())
    return SyncScreenBehavior::SHOW;

  return SyncScreenBehavior::UNKNOWN;
}

void SyncConsentScreen::UpdateScreen() {
  const SyncScreenBehavior new_behavior = GetSyncScreenBehavior();
  if (new_behavior == SyncScreenBehavior::UNKNOWN)
    return;

  const SyncScreenBehavior old_behavior = behavior_;
  behavior_ = new_behavior;

  if (!shown_ || behavior_ == old_behavior)
    return;

  // Screen is shown and behavior has changed.
  if (behavior_ == SyncScreenBehavior::SKIP)
    exit_callback_.Run();

  if (behavior_ == SyncScreenBehavior::SHOW) {
    view_->SetThrobberVisible(false /*visible*/);
    GetSyncService(profile_)->RemoveObserver(this);
  }
}

void SyncConsentScreen::RecordConsent(
    ConsentGiven consent_given,
    const std::vector<int>& consent_description,
    int consent_confirmation) {
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile_);
  const std::string& google_account_id =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountId();
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
