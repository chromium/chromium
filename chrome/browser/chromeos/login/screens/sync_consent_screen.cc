// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace {

constexpr const char kUserActionContinueWithSyncOnly[] =
    "continue-with-sync-only";
constexpr const char kUserActionContinueWithSyncAndPersonalization[] =
    "continue-with-sync-and-personalization";

browser_sync::ProfileSyncService* GetSyncService(Profile* profile) {
  if (ProfileSyncServiceFactory::HasProfileSyncService(profile))
    return ProfileSyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

}  // namespace

// static
void SyncConsentScreen::MaybeLaunchSyncConstentSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kShowSyncSettingsOnSessionStart)) {
    profile->GetPrefs()->ClearPref(prefs::kShowSyncSettingsOnSessionStart);
    chrome::ShowSettingsSubPageForProfile(profile, "syncSetup");
  }
}

SyncConsentScreen::SyncConsentScreen(BaseScreenDelegate* base_screen_delegate,
                                     SyncConsentScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_SYNC_CONSENT),
      view_(view) {
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
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
    return;
  }

  shown_ = true;
  if (behavior_ != SyncScreenBehavior::SHOW) {
    // Wait for updates and set the loading throbber to be visible.
    view_->SetThrobberVisible(true /*visible*/);
    GetSyncService(profile_)->AddObserver(this);
  }
  // Show the entire screen.
  // If SyncScreenBehavior is show, this should show the sync consent screen.
  // If SyncScreenBehavior is unknown, this should show the loading throbber.
  view_->Show();
}

void SyncConsentScreen::Hide() {
  shown_ = false;
  GetSyncService(profile_)->RemoveObserver(this);
  view_->Hide();
}

void SyncConsentScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinueWithSyncOnly) {
    // TODO(alemate) https://crbug.com/822889
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
    return;
  }
  if (action_id == kUserActionContinueWithSyncAndPersonalization) {
    // TODO(alemate) https://crbug.com/822889
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

void SyncConsentScreen::OnStateChanged(syncer::SyncService* sync) {
  UpdateScreen();
}

void SyncConsentScreen::OnContinueAndReview(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  RecordConsent(consent_description, consent_confirmation);
  profile_->GetPrefs()->SetBoolean(prefs::kShowSyncSettingsOnSessionStart,
                                   true);
  Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
}

void SyncConsentScreen::OnContinueWithDefaults(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  RecordConsent(consent_description, consent_confirmation);
  Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
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
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);

  if (behavior_ == SyncScreenBehavior::SHOW) {
    view_->SetThrobberVisible(false /*visible*/);
    GetSyncService(profile_)->RemoveObserver(this);
  }
}

void SyncConsentScreen::RecordConsent(
    const std::vector<int>& consent_description,
    const int consent_confirmation) {
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile_);
  const std::string& google_account_id =
      SigninManagerFactory::GetForProfile(profile_)
          ->GetAuthenticatedAccountId();
  // TODO(alemate): Support unified_consent_enabled
  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(consent_confirmation);
  for (int id : consent_description) {
    sync_consent.add_description_grd_ids(id);
  }
  sync_consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                              UserConsentTypes_ConsentStatus_GIVEN);
  consent_auditor->RecordSyncConsent(google_account_id, sync_consent);

  if (test_delegate_) {
    test_delegate_->OnConsentRecordedIds(consent_description,
                                         consent_confirmation);
  }
}

bool SyncConsentScreen::IsProfileSyncDisabledByPolicy() const {
  if (test_sync_disabled_by_policy_.has_value())
    return test_sync_disabled_by_policy_.value();
  const browser_sync::ProfileSyncService* sync_service =
      GetSyncService(profile_);
  return sync_service->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

bool SyncConsentScreen::IsProfileSyncEngineInitialized() const {
  if (test_sync_engine_initialized_.has_value())
    return test_sync_engine_initialized_.value();
  const browser_sync::ProfileSyncService* sync_service =
      GetSyncService(profile_);
  return sync_service->IsEngineInitialized();
}

void SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(bool value) {
  test_sync_disabled_by_policy_ = value;
}
void SyncConsentScreen::SetProfileSyncEngineInitializedForTesting(bool value) {
  test_sync_engine_initialized_ = value;
}

}  // namespace chromeos
