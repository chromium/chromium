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

ProfileManagementDisclaimerService::ProfileManagementDisclaimerService(
    Profile* profile)
    : profile_(*profile), state_(std::make_unique<ResetableState>()) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnforceManagementDisclaimer));

  scoped_identity_manager_observation_.Observe(GetIdentityManager());
  scoped_browser_list_observation_.Observe(BrowserList::GetInstance());

  MaybeShowEnterpriseManagementDisclaimer(
      GetPrimaryAccountInfo().account_id,
      signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup);
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
  MaybeShowEnterpriseManagementDisclaimer(account_id, access_point);
}

const CoreAccountId& ProfileManagementDisclaimerService::
    GetAccountBeingConsideredForManagementIfAny() const {
  return state_->account_id;
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
  // We can only create one managed profile at a time.
  CHECK(state_->account_id.empty() || state_->account_id == account_id);
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
  if (info.CanApplyAccountLevelEnterprisePolicies() ==
      signin::Tribool::kUnknown) {
    state_->extended_account_info_wait_timeout.Start(
        FROM_HERE, base::Seconds(5),
        base::BindOnce(&ProfileManagementDisclaimerService::Reset,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Account not managed, nothing to do.
  if (!signin::TriboolToBoolOrDie(
          info.CanApplyAccountLevelEnterprisePolicies())) {
    Reset();
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(&profile_.get());
  bool has_browser_with_tab =
      browser && browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
  // If there is no browser and we are not in tests, abort.
  if (!has_browser_with_tab && !profile_separation_policies_for_testing_ &&
      !user_choice_for_testing_) {
    Reset();
    return;
  }

  if (profile_separation_policies_for_testing_.has_value() ||
      user_choice_for_testing_.has_value()) {
    CHECK_IS_TEST();
    state_->profile_creation_controller =
        ManagedProfileCreationController::CreateManagedProfileForTesting(
            &profile_.get(), info, state_->access_point,
            base::BindOnce(&ProfileManagementDisclaimerService::
                               OnManagedProfileCreationResult,
                           weak_ptr_factory_.GetWeakPtr()),
            std::move(profile_separation_policies_for_testing_),
            std::move(user_choice_for_testing_));
  } else {
    state_->profile_creation_controller =
        ManagedProfileCreationController::CreateManagedProfile(
            &profile_.get(), info, state_->access_point,
            base::BindOnce(&ProfileManagementDisclaimerService::
                               OnManagedProfileCreationResult,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProfileManagementDisclaimerService::OnManagedProfileCreationResult(
    base::expected<Profile*, ManagedProfileCreationFailureReason> result,
    bool profile_creation_required_by_policy) {
  if (result.has_value() && result.value()) {
    state_->profile_to_continue_in = result.value()->GetWeakPtr();
  }
  state_->profile_creation_required_by_policy =
      profile_creation_required_by_policy;
  Reset();
}

void ProfileManagementDisclaimerService::Reset() {
  state_ = std::make_unique<ResetableState>();
}

void ProfileManagementDisclaimerService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
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
  if (info.CanApplyAccountLevelEnterprisePolicies() ==
      signin::Tribool::kUnknown) {
    return;
  }
  state_->extended_account_info_wait_timeout.Stop();
  MaybeShowEnterpriseManagementDisclaimer(info.account_id,
                                          state_->access_point);
}

void ProfileManagementDisclaimerService::OnBrowserSetLastActive(
    Browser* browser) {
  if (browser->profile() != &profile_.get()) {
    return;
  }
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
