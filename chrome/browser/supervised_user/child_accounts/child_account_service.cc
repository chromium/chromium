// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/browser/permission_request_creator_impl.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {
using ::base::BindRepeating;
using ::supervised_user::ListFamilyMembersService;
}  // namespace

ChildAccountService::ChildAccountService(
    Profile* profile,
    ListFamilyMembersService* list_family_members_service)
    : profile_(profile),
      list_family_members_service_(list_family_members_service),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {
  set_family_members_subscription_ =
      list_family_members_service_->SubscribeToSuccessfulFetches(BindRepeating(
          &supervised_user::RegisterFamilyPrefs,
          std::ref(*profile->GetPrefs())));  // list_family_members_service_ is
                                             // an instance of a keyed service
                                             // and PrefService outlives it.
}

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

bool ChildAccountService::IsChildAccountStatusKnown() {
  return supervised_user::IsChildAccountStatusKnown(*profile_->GetPrefs());
}

void ChildAccountService::Shutdown() {
  list_family_members_service_->Cancel();

  identity_manager_->RemoveObserver(this);
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(nullptr);
  DCHECK(!active_);
}

void ChildAccountService::AddChildStatusReceivedCallback(
    base::OnceClosure callback) {
  if (supervised_user::IsChildAccountStatusKnown(*profile_->GetPrefs())) {
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
    list_family_members_service_->Start();

    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    std::unique_ptr<supervised_user::PermissionRequestCreator> creator;
    if (base::FeatureList::IsEnabled(
            supervised_user::kEnableCreatePermissionRequestFetcher)) {
      creator = std::make_unique<supervised_user::PermissionRequestCreatorImpl>(
          identity_manager_, profile_->GetURLLoaderFactory());
    } else {
      creator = PermissionRequestCreatorApiary::CreateWithProfile(profile_);
    }
    service->remote_web_approvals_manager().AddApprovalRequestCreator(
        std::move(creator));
  } else {
    list_family_members_service_->Cancel();
  }
}

void ChildAccountService::SetSupervisionStatusAndNotifyObservers(
    bool supervision_status) {
  if (profile_->IsChild() != supervision_status) {
    if (supervision_status) {
      supervised_user::EnableParentalControls(*profile_->GetPrefs());
    } else {
      supervised_user::DisableParentalControls(*profile_->GetPrefs());
    }
  }

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
