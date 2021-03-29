// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#else
#include "chrome/browser/signin/signin_util.h"
#endif

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
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {}

ChildAccountService::~ChildAccountService() {}

// static
bool ChildAccountService::IsChildAccountDetectionEnabled() {
// Child account detection is always enabled on Android and ChromeOS, and
// disabled in other platforms.
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
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
  identity_manager_->AddObserver(this);

  PropagateChildStatusToUser(profile_->IsChild());

  // If we're already signed in, check the account immediately just to be sure.
  // (We might have missed an update before registering as an observer.)
  // "Unconsented" because this class doesn't care about browser sync consent.
  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));

  if (primary_account_info.has_value())
    OnExtendedAccountInfoUpdated(primary_account_info.value());
}

bool ChildAccountService::IsChildAccountStatusKnown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kChildAccountStatusKnown);
}

void ChildAccountService::Shutdown() {
  family_fetcher_.reset();
  identity_manager_->RemoveObserver(this);
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
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (!accounts_in_cookie_jar_info.accounts_are_fresh)
    return AuthState::PENDING;

  bool first_account_authenticated =
      !accounts_in_cookie_jar_info.signed_in_accounts.empty() &&
      accounts_in_cookie_jar_info.signed_in_accounts[0].valid;

  return first_account_authenticated ? AuthState::AUTHENTICATED
                                     : AuthState::NOT_AUTHENTICATED;
}

base::CallbackListSubscription ChildAccountService::ObserveGoogleAuthState(
    const base::RepeatingCallback<void()>& callback) {
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
        SupervisedUserSettingsServiceFactory::GetForKey(
            profile_->GetProfileKey());

    // In contrast to deprecated legacy SUs, child account SUs must sign in.
    settings_service->SetLocalSetting(supervised_users::kSigninAllowed,
                                      std::make_unique<base::Value>(true));

    // Always allow cookies, to avoid website compatibility issues.
    settings_service->SetLocalSetting(supervised_users::kCookiesAlwaysAllowed,
                                      std::make_unique<base::Value>(true));

    // SafeSearch is controlled at the account level, so don't override it
    // client-side.
    settings_service->SetLocalSetting(supervised_users::kForceSafeSearch,
                                      std::make_unique<base::Value>(false));

    // GeolocationDisabled is controlled at the account level, so don't override
    // it client-side.
    settings_service->SetLocalSetting(supervised_users::kGeolocationDisabled,
                                      std::make_unique<base::Value>(false));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // This is also used by user policies (UserPolicySigninService), but since
    // child accounts can not also be Dasher accounts, there shouldn't be any
    // problems.
    signin_util::SetUserSignoutAllowedForProfile(profile_, false);
#endif

    StartFetchingFamilyInfo();

    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    service->AddPermissionRequestCreator(
        PermissionRequestCreatorApiary::CreateWithProfile(profile_));
  } else {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(
            profile_->GetProfileKey());
    settings_service->SetLocalSetting(supervised_users::kSigninAllowed,
                                      nullptr);
    settings_service->SetLocalSetting(supervised_users::kCookiesAlwaysAllowed,
                                      nullptr);
    settings_service->SetLocalSetting(supervised_users::kForceSafeSearch,
                                      nullptr);
    settings_service->SetLocalSetting(supervised_users::kGeolocationDisabled,
                                      nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    signin_util::SetUserSignoutAllowedForProfile(profile_, true);
#endif

    CancelFetchingFamilyInfo();
  }

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the SupervisedUserSyncModelTypeController.
  // TODO(crbug.com/946473): Get rid of this hack and instead call
  // DataTypePreconditionChanged from the controller.
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service->GetUserSettings()->IsFirstSetupComplete()) {
    // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and
    // immediately releasing it again (via the temporary unique_ptr going away).
    sync_service->GetSetupInProgressHandle();
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

void ChildAccountService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    auto account_info =
        identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
            event_details.GetCurrentState().primary_account);
    if (account_info.has_value()) {
      OnExtendedAccountInfoUpdated(account_info.value());
    }
    // Otherwise OnExtendedAccountInfoUpdated will be notified once
    // the account info is available.
  }
}

void ChildAccountService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  // This method may get called when the account info isn't complete yet.
  // We deliberately don't check for that, as we are only interested in the
  // child account status.

  if (!IsChildAccountDetectionEnabled()) {
    SetIsChildAccount(false);
    return;
  }

  // This class doesn't care about browser sync consent.
  CoreAccountId auth_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (info.account_id != auth_account_id)
    return;

  SetIsChildAccount(info.is_child_account);
}

void ChildAccountService::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // This class doesn't care about browser sync consent.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin))
    return;

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
  DLOG(WARNING) << "GetFamilyMembers failed with code "
                << static_cast<int>(error);
  family_fetch_backoff_.InformOfRequest(false);
  ScheduleNextFamilyInfoUpdate(family_fetch_backoff_.GetTimeUntilRelease());
}

void ChildAccountService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  google_auth_state_observers_.Notify();
}

void ChildAccountService::StartFetchingFamilyInfo() {
  family_fetcher_.reset(new FamilyInfoFetcher(
      this, identity_manager_,
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    // Note that deprecated legacy supervised users are allowed to change type
    // due to legacy initialization.
    if (user->GetType() != user_manager::USER_TYPE_SUPERVISED_DEPRECATED) {
      if (is_child != (user->GetType() == user_manager::USER_TYPE_CHILD))
        LOG(FATAL) << "User child flag has changed: " << is_child;
    }
  } else if (chromeos::ProfileHelper::IsRegularProfile(profile_)) {
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
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserCustodianObfuscatedGaiaId,
      custodian.obfuscated_gaia_id);
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
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
      custodian.obfuscated_gaia_id);
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
