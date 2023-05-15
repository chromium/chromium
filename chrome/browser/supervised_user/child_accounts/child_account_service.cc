// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/kids_external_fetcher.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {

// How often to refetch the family members.
constexpr base::TimeDelta kUpdateInterval = base::Days(1);

// In case of an error while getting the family info, retry with exponential
// backoff.
const net::BackoffEntry::Policy kFamilyFetchBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    .num_errors_to_ignore = 0,

    // Initial delay for exponential backoff in ms.
    .initial_delay_ms = 2000,

    // Factor by which the waiting time will be multiplied.
    .multiply_factor = 2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    .jitter_factor = 0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    .maximum_backoff_ms = 1000 * 60 * 60 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    .entry_lifetime_ms = -1,

    // Don't use initial delay unless the last request was an error.
    .always_use_initial_delay = false,
};

}  // namespace

ChildAccountService::ChildAccountService(Profile* profile)
    : profile_(profile),
      family_fetch_backoff_(&kFamilyFetchBackoffPolicy),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {}

ChildAccountService::~ChildAccountService() = default;

void ChildAccountService::Init() {
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(this);
  identity_manager_->AddObserver(this);

  AssertChildStatusOfTheUser(profile_->IsChild());

  // If we're already signed in, check the account immediately just to be sure.
  // (We might have missed an update before registering as an observer.)
  // "Unconsented" because this class doesn't care about browser sync consent.
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty()) {
    OnExtendedAccountInfoUpdated(primary_account_info);
  }
}

void ChildAccountService::Shutdown() {
  CancelFetchingFamilyInfo();

  identity_manager_->RemoveObserver(this);
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(nullptr);
  DCHECK(!active_);
}

void ChildAccountService::AddChildStatusReceivedCallback(
    base::OnceClosure callback) {
  if (IsChildAccountStatusKnown()) {
    std::move(callback).Run();
  } else {
    status_received_callback_list_.push_back(std::move(callback));
  }
}

ChildAccountService::AuthState ChildAccountService::GetGoogleAuthState() {
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (!accounts_in_cookie_jar_info.accounts_are_fresh) {
    return AuthState::PENDING;
  }

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

void ChildAccountService::SetActive(bool active) {
  if (!profile_->IsChild() && !active_) {
    return;
  }
  if (active_ == active) {
    return;
  }
  active_ = active;

  if (active_) {
    StartFetchingFamilyInfo();

    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    service->remote_web_approvals_manager().AddApprovalRequestCreator(
        PermissionRequestCreatorApiary::CreateWithProfile(profile_));
  } else {
    CancelFetchingFamilyInfo();
  }
}

void ChildAccountService::SetSupervisionStatusAndNotifyObservers(
    bool is_child_account) {
  if (profile_->IsChild() != is_child_account) {
    SetIsSubjectToParentalControls(is_child_account);
    if (!is_child_account) {
      ClearCustodianPrefs(first_custodian);
      ClearCustodianPrefs(second_custodian);
    }
  }
  SetIsChildAccountStatusKnown();

  for (auto& callback : status_received_callback_list_) {
    std::move(callback).Run();
  }
  status_received_callback_list_.clear();
}

void ChildAccountService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
        event_details.GetCurrentState().primary_account);
    if (!account_info.IsEmpty()) {
      OnExtendedAccountInfoUpdated(account_info);
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
    SetSupervisionStatusAndNotifyObservers(false);
    return;
  }

  // This class doesn't care about browser sync consent.
  CoreAccountId auth_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (info.account_id != auth_account_id) {
    return;
  }

  SetSupervisionStatusAndNotifyObservers(info.is_child_account ==
                                         signin::Tribool::kTrue);
}

void ChildAccountService::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // This class doesn't care about browser sync consent.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  SetSupervisionStatusAndNotifyObservers(false);
}

void ChildAccountService::OnResponse(
    KidsExternalFetcherStatus status,
    std::unique_ptr<kids_chrome_management::ListFamilyMembersResponse>
        response) {
  if (!status.IsOk()) {
    OnFailure(status);
    return;
  }

  OnSuccess(*response);
  // Release response.
}

void ChildAccountService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  google_auth_state_observers_.Notify();
}

void ChildAccountService::AssertChildStatusOfTheUser(bool is_child) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user && is_child != (user->GetType() == user_manager::USER_TYPE_CHILD)) {
    LOG(FATAL) << "User child flag has changed: " << is_child;
  }
  if (!user && ash::ProfileHelper::IsUserProfile(profile_)) {
    LOG(DFATAL) << "User instance not found while setting child account flag.";
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  bool is_child_session = chromeos::BrowserParamsProxy::Get()->SessionType() ==
                          crosapi::mojom::SessionType::kChildSession;
  if (is_child_session != is_child) {
    LOG(FATAL) << "User child flag has changed: " << is_child;
  }
#endif
}

// The following methods set and clear user & custodian information in the
// profile preferences.
void ChildAccountService::SetIsSubjectToParentalControls(
    bool is_subject_to_parental_controls) {
  if (is_subject_to_parental_controls) {
    profile_->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                    supervised_user::kChildAccountSUID);
  } else {
    profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserId);
  }
}

void ChildAccountService::SetIsChildAccountStatusKnown() {
  profile_->GetPrefs()->SetBoolean(prefs::kChildAccountStatusKnown, true);
}

bool ChildAccountService::IsChildAccountStatusKnown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kChildAccountStatusKnown);
}

void ChildAccountService::SetCustodianPrefs(
    const Custodian& custodian,
    const kids_chrome_management::FamilyMember& member) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetString(custodian.display_name, member.profile().display_name());
  prefs->SetString(custodian.email, member.profile().email());
  prefs->SetString(custodian.user_id, member.user_id());
  prefs->SetString(custodian.profile_url, member.profile().profile_url());
  prefs->SetString(custodian.profile_image_url,
                   member.profile().profile_image_url());
}

void ChildAccountService::ClearCustodianPrefs(const Custodian& custodian) {
  profile_->GetPrefs()->ClearPref(custodian.display_name);
  profile_->GetPrefs()->ClearPref(custodian.email);
  profile_->GetPrefs()->ClearPref(custodian.user_id);
  profile_->GetPrefs()->ClearPref(custodian.profile_url);
  profile_->GetPrefs()->ClearPref(custodian.profile_image_url);
}

// The following methods handle the fetching of list family members.
void ChildAccountService::OnSuccess(
    const kids_chrome_management::ListFamilyMembersResponse& response) {
  bool hoh_found = false;
  bool parent_found = false;
  for (const kids_chrome_management::FamilyMember& member :
       response.members()) {
    if (member.role() == kids_chrome_management::HEAD_OF_HOUSEHOLD) {
      hoh_found = true;
      SetCustodianPrefs(first_custodian, member);
    } else if (member.role() == kids_chrome_management::PARENT) {
      parent_found = true;
      SetCustodianPrefs(second_custodian, member);
    }
    if (hoh_found && parent_found) {
      break;
    }
  }
  if (!hoh_found) {
    DLOG(WARNING) << "ListFamilyMembers didn't return a Head of household.";
    ClearCustodianPrefs(first_custodian);
  }
  if (!parent_found) {
    ClearCustodianPrefs(second_custodian);
  }

  list_family_members_fetcher_.reset();
  family_fetch_backoff_.InformOfRequest(true);

  ScheduleNextFamilyInfoUpdate(kUpdateInterval);
}

void ChildAccountService::OnFailure(KidsExternalFetcherStatus error) {
  DLOG(WARNING) << "ListFamilyMembers failed with status " << error.ToString();
  family_fetch_backoff_.InformOfRequest(false);
  ScheduleNextFamilyInfoUpdate(family_fetch_backoff_.GetTimeUntilRelease());
}

void ChildAccountService::StartFetchingFamilyInfo() {
  list_family_members_fetcher_ = FetchListFamilyMembers(
      *identity_manager_, profile_->GetURLLoaderFactory(),
      base::BindOnce(&ChildAccountService::OnResponse, base::Unretained(this)));
}

void ChildAccountService::CancelFetchingFamilyInfo() {
  list_family_members_fetcher_.reset();
  family_fetch_timer_.Stop();
}

void ChildAccountService::ScheduleNextFamilyInfoUpdate(base::TimeDelta delay) {
  family_fetch_timer_.Start(FROM_HERE, delay, this,
                            &ChildAccountService::StartFetchingFamilyInfo);
}
