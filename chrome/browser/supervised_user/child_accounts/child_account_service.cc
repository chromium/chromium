// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/experimental/safe_search_url_reporter.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#else
#include "chrome/browser/signin/signin_util.h"
#endif

const char kGaiaCookieManagerSource[] = "child_account_service";

// Normally, re-check the family info once per day.
const int kUpdateIntervalSeconds = 60 * 60 * 24;

// In case of an error while getting the family info, retry with exponential
// backoff.
const net::BackoffEntry::Policy kFamilyFetchBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential backoff in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 60 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

ChildAccountService::ChildAccountService(Profile* profile)
    : profile_(profile),
      active_(false),
      family_fetch_backoff_(&kFamilyFetchBackoffPolicy),
      gaia_cookie_manager_(
          GaiaCookieManagerServiceFactory::GetForProfile(profile)),
      weak_ptr_factory_(this) {
  gaia_cookie_manager_->AddObserver(this);
}

ChildAccountService::~ChildAccountService() {
  gaia_cookie_manager_->RemoveObserver(this);
}

// static
bool ChildAccountService::IsChildAccountDetectionEnabled() {
// Child account detection is always enabled on Android and ChromeOS, and
// disabled in other platforms.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

void ChildAccountService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kChildAccountStatusKnown, false);
}

void ChildAccountService::Init() {
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(this);
  AccountTrackerServiceFactory::GetForProfile(profile_)->AddObserver(this);

  PropagateChildStatusToUser(profile_->IsChild());

  // If we're already signed in, check the account immediately just to be sure.
  // (We might have missed an update before registering as an observer.)
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile_);
  if (signin->IsAuthenticated()) {
    OnAccountUpdated(
        AccountTrackerServiceFactory::GetForProfile(profile_)->GetAccountInfo(
            signin->GetAuthenticatedAccountId()));
  }
}

bool ChildAccountService::IsChildAccountStatusKnown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kChildAccountStatusKnown);
}

void ChildAccountService::Shutdown() {
  family_fetcher_.reset();
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(nullptr);
  DCHECK(!active_);
}

void ChildAccountService::AddChildStatusReceivedCallback(
    base::OnceClosure callback) {
  if (IsChildAccountStatusKnown())
    std::move(callback).Run();
  else
    status_received_callback_list_.push_back(std::move(callback));
}

ChildAccountService::AuthState ChildAccountService::GetGoogleAuthState() {
  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  if (!gaia_cookie_manager_->ListAccounts(&accounts, &signed_out_accounts,
                                          kGaiaCookieManagerSource)) {
    return AuthState::PENDING;
  }
  return (accounts.empty() || !accounts[0].valid) ? AuthState::NOT_AUTHENTICATED
                                                  : AuthState::AUTHENTICATED;
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
ChildAccountService::ObserveGoogleAuthState(
    const base::Callback<void()>& callback) {
  return google_auth_state_observers_.Add(callback);
}

bool ChildAccountService::SetActive(bool active) {
  if (!profile_->IsChild() && !active_)
    return false;
  if (active_ == active)
    return true;
  active_ = active;

  if (active_) {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile_);

    settings_service->SetLocalSetting(
        supervised_users::kRecordHistoryIncludesSessionSync,
        std::make_unique<base::Value>(false));

    // In contrast to legacy SUs, child account SUs must sign in.
    settings_service->SetLocalSetting(supervised_users::kSigninAllowed,
                                      std::make_unique<base::Value>(true));

    // Always allow cookies, to avoid website compatibility issues.
    settings_service->SetLocalSetting(supervised_users::kCookiesAlwaysAllowed,
                                      std::make_unique<base::Value>(true));

    // SafeSearch is controlled at the account level, so don't override it
    // client-side.
    settings_service->SetLocalSetting(supervised_users::kForceSafeSearch,
                                      std::make_unique<base::Value>(false));

#if defined(OS_CHROMEOS)
    // Mirror account consistency is required for child accounts on Chrome OS.
    settings_service->SetLocalSetting(
        supervised_users::kAccountConsistencyMirrorRequired,
        std::make_unique<base::Value>(true));
#endif

#if !defined(OS_CHROMEOS)
    // This is also used by user policies (UserPolicySigninService), but since
    // child accounts can not also be Dasher accounts, there shouldn't be any
    // problems.
    signin_util::SetUserSignoutAllowedForProfile(profile_, false);
#endif

    // TODO(treib): Maybe store the last update time in a pref, so we don't
    // have to re-fetch on every start.
    StartFetchingFamilyInfo();

    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    service->AddPermissionRequestCreator(
        PermissionRequestCreatorApiary::CreateWithProfile(profile_));
    if (base::FeatureList::IsEnabled(features::kSafeSearchUrlReporting)) {
      service->SetSafeSearchURLReporter(
          SafeSearchURLReporter::CreateWithProfile(profile_));
    }
  } else {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile_);
    settings_service->SetLocalSetting(
        supervised_users::kRecordHistoryIncludesSessionSync, nullptr);
    settings_service->SetLocalSetting(supervised_users::kSigninAllowed,
                                      nullptr);
    settings_service->SetLocalSetting(supervised_users::kCookiesAlwaysAllowed,
                                      nullptr);
    settings_service->SetLocalSetting(supervised_users::kForceSafeSearch,
                                      nullptr);
#if defined(OS_CHROMEOS)
    settings_service->SetLocalSetting(
        supervised_users::kAccountConsistencyMirrorRequired, nullptr);
#endif

#if !defined(OS_CHROMEOS)
    signin_util::SetUserSignoutAllowedForProfile(profile_, true);
#endif

    CancelFetchingFamilyInfo();
  }

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the SupervisedUserSyncDataTypeController.
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service->IsFirstSetupComplete()) {
    sync_service->ReconfigureDatatypeManager(
        /*bypass_setup_in_progress_check=*/false);
  }

  return true;
}

void ChildAccountService::SetIsChildAccount(bool is_child_account) {
  if (profile_->IsChild() != is_child_account) {
    if (is_child_account) {
      profile_->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                      supervised_users::kChildAccountSUID);
    } else {
      profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserId);

      ClearFirstCustodianPrefs();
      ClearSecondCustodianPrefs();
    }
  }
  profile_->GetPrefs()->SetBoolean(prefs::kChildAccountStatusKnown, true);

  for (auto& callback : status_received_callback_list_)
    std::move(callback).Run();
  status_received_callback_list_.clear();
}

void ChildAccountService::OnAccountUpdated(const AccountInfo& info) {
  // This method may get called when the account info isn't complete yet.
  // We deliberately don't check for that, as we are only interested in the
  // child account status.

  if (!IsChildAccountDetectionEnabled()) {
    SetIsChildAccount(false);
    return;
  }

  std::string auth_account_id = SigninManagerFactory::GetForProfile(profile_)
      ->GetAuthenticatedAccountId();
  if (info.account_id != auth_account_id)
    return;

  SetIsChildAccount(info.is_child_account);
}

void ChildAccountService::OnAccountRemoved(const AccountInfo& info) {
  SetIsChildAccount(false);
}

void ChildAccountService::OnGetFamilyMembersSuccess(
    const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
  bool hoh_found = false;
  bool parent_found = false;
  for (const FamilyInfoFetcher::FamilyMember& member : members) {
    if (member.role == FamilyInfoFetcher::HEAD_OF_HOUSEHOLD) {
      hoh_found = true;
      SetFirstCustodianPrefs(member);
    } else if (member.role == FamilyInfoFetcher::PARENT) {
      parent_found = true;
      SetSecondCustodianPrefs(member);
    }
    if (hoh_found && parent_found)
      break;
  }
  if (!hoh_found) {
    DLOG(WARNING) << "GetFamilyMembers didn't return a HOH?!";
    ClearFirstCustodianPrefs();
  }
  if (!parent_found)
    ClearSecondCustodianPrefs();
  family_fetcher_.reset();

  family_fetch_backoff_.InformOfRequest(true);

  ScheduleNextFamilyInfoUpdate(
      base::TimeDelta::FromSeconds(kUpdateIntervalSeconds));
}

void ChildAccountService::OnFailure(FamilyInfoFetcher::ErrorCode error) {
  DLOG(WARNING) << "GetFamilyMembers failed with code " << error;
  family_fetch_backoff_.InformOfRequest(false);
  ScheduleNextFamilyInfoUpdate(family_fetch_backoff_.GetTimeUntilRelease());
}

void ChildAccountService::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  google_auth_state_observers_.Notify();
}

void ChildAccountService::StartFetchingFamilyInfo() {
  family_fetcher_.reset(new FamilyInfoFetcher(
      this,
      SigninManagerFactory::GetForProfile(profile_)
          ->GetAuthenticatedAccountId(),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess()));
  family_fetcher_->StartGetFamilyMembers();
}

void ChildAccountService::CancelFetchingFamilyInfo() {
  family_fetcher_.reset();
  family_fetch_timer_.Stop();
}

void ChildAccountService::ScheduleNextFamilyInfoUpdate(base::TimeDelta delay) {
  family_fetch_timer_.Start(
      FROM_HERE, delay, this, &ChildAccountService::StartFetchingFamilyInfo);
}

void ChildAccountService::PropagateChildStatusToUser(bool is_child) {
#if defined(OS_CHROMEOS)
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    // Note that supervised user is allowed to change type due to legacy
    // initialization.
    if (user->GetType() != user_manager::USER_TYPE_SUPERVISED) {
      if (is_child != (user->GetType() == user_manager::USER_TYPE_CHILD))
        LOG(FATAL) << "User child flag has changed: " << is_child;
    }
  } else if (!chromeos::ProfileHelper::Get()->IsSigninProfile(profile_) &&
             !chromeos::ProfileHelper::Get()->IsLockScreenAppProfile(
                 profile_)) {
    LOG(DFATAL) << "User instance not found while setting child account flag.";
  }
#endif
}

void ChildAccountService::SetFirstCustodianPrefs(
    const FamilyInfoFetcher::FamilyMember& custodian) {
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianName,
                                  custodian.display_name);
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianEmail,
                                  custodian.email);
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianProfileURL,
                                  custodian.profile_url);
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserCustodianProfileImageURL,
      custodian.profile_image_url);
}

void ChildAccountService::SetSecondCustodianPrefs(
    const FamilyInfoFetcher::FamilyMember& custodian) {
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserSecondCustodianName,
                                  custodian.display_name);
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                                  custodian.email);
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianProfileURL,
      custodian.profile_url);
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL,
      custodian.profile_image_url);
}

void ChildAccountService::ClearFirstCustodianPrefs() {
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianName);
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianEmail);
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianProfileURL);
  profile_->GetPrefs()->ClearPref(
      prefs::kSupervisedUserCustodianProfileImageURL);
}

void ChildAccountService::ClearSecondCustodianPrefs() {
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserSecondCustodianName);
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserSecondCustodianEmail);
  profile_->GetPrefs()->ClearPref(
      prefs::kSupervisedUserSecondCustodianProfileURL);
  profile_->GetPrefs()->ClearPref(
      prefs::kSupervisedUserSecondCustodianProfileImageURL);
}
