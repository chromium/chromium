// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"

#include <memory>

#include "base/check.h"
#include "base/check_is_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signin/managed_profile_creation_controller.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace {

bool CanTryPolicyRegistration(std::optional<base::Time> last_failure_time) {
  if (!last_failure_time) {
    return true;
  }

  return base::Time::Now() - last_failure_time.value() >
         switches::kPolicyDisclaimerRegistrationRetryDelay.Get();
}

bool IsSigninRegistration(signin_metrics::AccessPoint access_point) {
  return access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAtStartup &&
         access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAfterBrowserFocus;
}

bool AllowDisclaimer(signin_metrics::AccessPoint access_point) {
  if (base::FeatureList::IsEnabled(switches::kEnforceManagementDisclaimer)) {
    return true;
  }
  return access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAtStartup &&
         access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAfterBrowserFocus &&
         access_point != signin_metrics::AccessPoint::
                             kEnterpriseManagementDisclaimerAfterSignin;
}

}  // namespace

ProfileManagementDisclaimerService::ProfileManagementDisclaimerService(
    Profile* profile)
    : profile_(*profile),
      state_(std::make_unique<ResetableState>()),
      signin_prefs_(*profile->GetPrefs()) {
  scoped_identity_manager_observation_.Observe(GetIdentityManager());
  scoped_browser_collection_observation_.Observe(
      ProfileBrowserCollection::GetForProfile(profile));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ProfileManagementDisclaimerService::
                                    MaybeShowEnterpriseManagementDisclaimer,
                                weak_ptr_factory_.GetWeakPtr(),
                                GetPrimaryAccountInfo().account_id,
                                signin_metrics::AccessPoint::
                                    kEnterpriseManagementDisclaimerAtStartup));
}

ProfileManagementDisclaimerService::~ProfileManagementDisclaimerService() =
    default;

base::ScopedClosureRunner
ProfileManagementDisclaimerService::DisableManagementDisclaimerUntilReset() {
  if (!enable_management_disclaimer_) {
    return base::ScopedClosureRunner();
  }
  enable_management_disclaimer_ = false;
  return base::ScopedClosureRunner(
      base::BindOnce(&ProfileManagementDisclaimerService::
                         SetEnableManagementDisclaimerOnPrimaryAccountChange,
                     weak_ptr_factory_.GetWeakPtr(), true));
}

base::ScopedClosureRunner
ProfileManagementDisclaimerService::AutoAcceptManagementDisclaimerUntilReset() {
  active_auto_accept_count_++;
  auto_accept_management_ = true;
  return base::ScopedClosureRunner(base::BindOnce(
      &ProfileManagementDisclaimerService::MaybeResetAcceptManagementDisclaimer,
      weak_ptr_factory_.GetWeakPtr(), /*auto_accept_management=*/false));
}

void ProfileManagementDisclaimerService::MaybeResetAcceptManagementDisclaimer(
    bool auto_accept_management) {
  active_auto_accept_count_--;
  CHECK_GE(active_auto_accept_count_, 0);
  if (active_auto_accept_count_ == 0) {
    auto_accept_management_ = auto_accept_management;
  }
}

ProfileManagementDisclaimerService::ResetableState::ResetableState() = default;

ProfileManagementDisclaimerService::ResetableState::~ResetableState() {
  callbacks.Notify(profile_to_continue_in.get(),
                   profile_creation_required_by_policy);
}

void ProfileManagementDisclaimerService::EnsureManagedProfileForAccount(
    const CoreAccountId& account_id,
    signin_metrics::AccessPoint access_point,
    base::OnceCallback<void(Profile*, bool)> callback) {
  CHECK(state_->account_id.empty() || state_->account_id == account_id);
  state_->callbacks.AddUnsafe(std::move(callback));
  state_->cancelable = false;
  MaybeShowEnterpriseManagementDisclaimer(account_id, access_point);
}

const CoreAccountId& ProfileManagementDisclaimerService::
    GetAccountBeingConsideredForManagementIfAny() const {
  return state_->account_id;
}

bool ProfileManagementDisclaimerService::StopCurrentProcessIfPossible() {
  if (state_->profile_creation_controller) {
    return false;
  }
  if (!state_->cancelable) {
    return false;
  }
  Reset();
  return true;
}

signin::IdentityManager*
ProfileManagementDisclaimerService::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(&profile_.get());
}

AccountInfo ProfileManagementDisclaimerService::GetPrimaryAccountInfo() {
  auto* identity_manager = GetIdentityManager();
  CHECK(identity_manager);
  return identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
}

AccountInfo ProfileManagementDisclaimerService::GetExtendedAccountInfo(
    const CoreAccountId& account_id) {
  auto* identity_manager = GetIdentityManager();
  CHECK(identity_manager);
  return identity_manager->FindExtendedAccountInfoByAccountId(account_id);
}

void ProfileManagementDisclaimerService::
    MaybeShowEnterpriseManagementDisclaimer(
        const CoreAccountId& account_id,
        signin_metrics::AccessPoint access_point) {
  if (account_id.empty()) {
    return;
  }
  // We should always know the access point that triggered the profile creation.
  CHECK_NE(access_point, signin_metrics::AccessPoint::kUnknown);

  if (!AllowDisclaimer(access_point)) {
    return;
  }

  if (!state_->account_id.empty() && state_->account_id != account_id) {
    // If the account is different from the one we are already handling, reset
    // the state. This can happen if the account is removed and another one is
    // added, or if the account is cleared and another account is set as primary
    // account.
    return;
  }

  // If the management disclaimer is not enabled on primary account change,
  // reset the state and return early. This to avoid showing the disclaimer
  // after the primary account has changed when another class is handling
  // signin.
  if (!enable_management_disclaimer_) {
    Reset();
    return;
  }
  state_->access_point = access_point;

  // Wait for the current disclaimer to be closed.
  if (state_->profile_creation_controller) {
    return;
  }

  state_->account_id = account_id;

  // If the user has already accepted the management disclaimer, nothing to
  // show.
  if (enterprise_util::UserAcceptedAccountManagement(&profile_.get())) {
    state_->profile_to_continue_in = profile_->GetWeakPtr();
    Reset();
    return;
  }

  AccountInfo info = GetExtendedAccountInfo(account_id);

  // Account info is not yet available, wait for extended account info.
  if (info.IsManaged() == signin::Tribool::kUnknown) {
    state_->extended_account_info_wait_timeout.Start(
        FROM_HERE, base::Seconds(5),
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If there is no refresh token, we cannot register for policy updates.
  // Wait for it to be updated.
  if (!GetIdentityManager()->HasAccountWithRefreshToken(account_id)) {
    state_->refresh_token_wait_timeout.Start(
        FROM_HERE, base::Seconds(5),
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Account not managed, nothing to do.
  if (!signin::TriboolToBoolOrDie(info.IsManaged())) {
    Reset();
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(&profile_.get());
  bool has_browser_with_tab =
      browser &&
      browser->SupportsWindowFeature(Browser::WindowFeature::kFeatureTabStrip);
  // If there is no browser and we are not in tests, abort.
  if (!has_browser_with_tab && !profile_separation_policies_for_testing_ &&
      !user_choice_for_testing_) {
    Reset();
    return;
  }

  CHECK(!state_->profile_creation_controller);

  // If the account cannot try to register for policies because of delays
  // between failures, we can reset the state and wait for another attempt.
  if (!CanTryPolicyRegistration(
          signin_prefs_.GetPolicyDisclaimerLastRegistrationFailureTime(
              info.gaia))) {
    OnRegisteredForPolicy(/*is_from_cached_registration_result=*/true,
                          /*is_managed_account=*/false);
    return;
  }

  // If the account is already registered for policy, we can check the result
  // immediately. Otherwise, we need to register for policy updates.
  bool has_cached_successful_registration_result =
      policy_fetch_tracker_by_account_id_.contains(account_id) &&
      policy_fetch_tracker_by_account_id_[account_id]
          ->GetPolicyRegistrationResult()
          .value_or(false);

  if (has_cached_successful_registration_result) {
    OnRegisteredForPolicy(/*is_from_cached_registration_result=*/true,
                          /*is_managed_account=*/true);
    return;
  }

  // Create a new tracker for the account, if it doesn't exist yet or if it had
  // a cached failure. This will also reset any cached failure.
  policy_fetch_tracker_by_account_id_[account_id] =
      TurnSyncOnHelperPolicyFetchTracker::CreateInstance(&profile_.get(), info);

  policy_fetch_tracker_by_account_id_[account_id]->RegisterForPolicy(
      base::BindOnce(&ProfileManagementDisclaimerService::OnRegisteredForPolicy,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_from_cached_registration_result=*/false),
      !IsSigninRegistration(state_->access_point));
}

void ProfileManagementDisclaimerService::OnRegisteredForPolicy(
    bool is_from_cached_registration_result,
    bool is_managed_account) {
  if (!enable_management_disclaimer_) {
    Reset();
    return;
  }
  GaiaId gaia_id = GetExtendedAccountInfo(state_->account_id).gaia;
  // If the account has been removed in the meantime, reset the state.
  if (gaia_id.empty()) {
    state_->profile_to_continue_in = nullptr;
    Reset();
    return;
  }
  if (!is_managed_account) {
    if (!is_from_cached_registration_result) {
      signin_prefs_.SetPolicyDisclaimerLastRegistrationFailureTime(
          gaia_id, base::Time::Now());
    }
    Reset();
    return;
  }
  signin_prefs_.ClearPolicyDisclaimerLastRegistrationFailureTime(gaia_id);

  if (auto_accept_management_) {
    enterprise_util::SetUserAcceptedAccountManagement(&profile_.get(), true);
    OnManagedProfileCreationResult(
        base::ok<Profile*>(&profile_.get()),
        /*profile_creation_required_by_policy=*/false);
    return;
  }

  if (profile_separation_policies_for_testing_.has_value() ||
      user_choice_for_testing_.has_value()) {
    CHECK_IS_TEST();
    state_->profile_creation_controller =
        ManagedProfileCreationController::CreateManagedProfileForTesting(
            &profile_.get(), GetExtendedAccountInfo(state_->account_id),
            state_->access_point,
            base::BindOnce(&ProfileManagementDisclaimerService::
                               OnManagedProfileCreationResult,
                           weak_ptr_factory_.GetWeakPtr()),
            std::move(profile_separation_policies_for_testing_),
            std::move(user_choice_for_testing_));
    return;
  }

  state_->profile_creation_controller =
      ManagedProfileCreationController::CreateManagedProfile(
          &profile_.get(), GetExtendedAccountInfo(state_->account_id),
          state_->access_point,
          base::BindOnce(&ProfileManagementDisclaimerService::
                             OnManagedProfileCreationResult,
                         weak_ptr_factory_.GetWeakPtr()));
}

void ProfileManagementDisclaimerService::OnManagedProfileCreationResult(
    base::expected<Profile*, ManagedProfileCreationFailureReason> result,
    bool profile_creation_required_by_policy) {
  if (result.has_value() && result.value()) {
    state_->profile_to_continue_in = result.value()->GetWeakPtr();
  }
  state_->profile_creation_required_by_policy =
      profile_creation_required_by_policy;
  auto& policy_fetch_tracker =
      policy_fetch_tracker_by_account_id_[state_->account_id];
  if (state_->profile_to_continue_in && policy_fetch_tracker) {
    policy_fetch_tracker->SwitchToProfile(state_->profile_to_continue_in.get());
    policy_fetch_tracker->FetchPolicy(
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  Reset();
}

void ProfileManagementDisclaimerService::Reset() {
  state_ = std::make_unique<ResetableState>();
}

void ProfileManagementDisclaimerService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
          signin::PrimaryAccountChangeEvent::Type::kCleared &&
      state_->account_id == GetPrimaryAccountInfo().account_id) {
    state_->profile_to_continue_in = nullptr;
    Reset();
    return;
  }
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }

  // If we are already handling a signin, ignore this event.
  if (!state_->account_id.empty()) {
    return;
  }

  // Post the task here because the class that set the primary account might
  // handle the signin in a synchronous way. This avoids showing the disclaimer
  // twice.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProfileManagementDisclaimerService::
                         MaybeShowEnterpriseManagementDisclaimer,
                     weak_ptr_factory_.GetWeakPtr(),
                     event.GetCurrentState().primary_account.account_id,
                     signin_metrics::AccessPoint::
                         kEnterpriseManagementDisclaimerAfterSignin));
}

void ProfileManagementDisclaimerService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id != state_->account_id) {
    return;
  }
  // Management status is not yet available, wait for extended account info.
  if (info.IsManaged() == signin::Tribool::kUnknown) {
    return;
  }
  state_->extended_account_info_wait_timeout.Stop();
  MaybeShowEnterpriseManagementDisclaimer(info.account_id,
                                          state_->access_point);
}

void ProfileManagementDisclaimerService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (state_->access_point == signin_metrics::AccessPoint::kUnknown) {
    return;
  }
  // This would most likely happen at startup after all refresh tokens are
  // loaded.
  if (state_->account_id.empty() &&
      GetPrimaryAccountInfo().account_id != account_info.account_id) {
    return;
  }
  if (!state_->account_id.empty() &&
      account_info.account_id != state_->account_id) {
    return;
  }
  MaybeShowEnterpriseManagementDisclaimer(account_info.account_id,
                                          state_->access_point);
  state_->refresh_token_wait_timeout.Stop();
}

void ProfileManagementDisclaimerService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  CoreAccountId account_id = state_->account_id.empty()
                                 ? GetPrimaryAccountInfo().account_id
                                 : state_->account_id;
  signin_metrics::AccessPoint access_point =
      state_->access_point != signin_metrics::AccessPoint::kUnknown
          ? state_->access_point
          : signin_metrics::AccessPoint::
                kEnterpriseManagementDisclaimerAfterBrowserFocus;
  MaybeShowEnterpriseManagementDisclaimer(account_id, access_point);
}
