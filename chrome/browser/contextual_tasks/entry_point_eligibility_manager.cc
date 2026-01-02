// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace contextual_tasks {
DEFINE_USER_DATA(EntryPointEligibilityManager);

EntryPointEligibilityManager::EntryPointEligibilityManager(
    BrowserWindowInterface* browser_window_interface)
    : profile_(browser_window_interface->GetProfile()),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }

  aim_policy_.Init(
      omnibox::kAIModeSettings, profile_->GetPrefs(),
      base::BindRepeating(&EntryPointEligibilityManager::
                              MaybeNotifyEntryPointEligibilityChanged,
                          base::Unretained(this)));

  entry_points_are_eligible_ = AreEntryPointsEligible();
  AimEligibilityService* const aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);

  CHECK(aim_eligibility_service);
  aim_eligibility_callback_subscription_ =
      aim_eligibility_service->RegisterEligibilityChangedCallback(
          base::BindRepeating(&EntryPointEligibilityManager::
                                  MaybeNotifyEntryPointEligibilityChanged,
                              base::Unretained(this)));
  entry_points_are_eligible_ = AreEntryPointsEligible();
}

EntryPointEligibilityManager::~EntryPointEligibilityManager() = default;

EntryPointEligibilityManager* EntryPointEligibilityManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void EntryPointEligibilityManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  MaybeNotifyEntryPointEligibilityChanged();
}

void EntryPointEligibilityManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  MaybeNotifyEntryPointEligibilityChanged();
}

void EntryPointEligibilityManager::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  MaybeNotifyEntryPointEligibilityChanged();
}

void EntryPointEligibilityManager::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  MaybeNotifyEntryPointEligibilityChanged();
}

void EntryPointEligibilityManager::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  MaybeNotifyEntryPointEligibilityChanged();
}

void EntryPointEligibilityManager::OnAccountsCookieDeletedByUserAction() {
  MaybeNotifyEntryPointEligibilityChanged();
}

base::CallbackListSubscription
EntryPointEligibilityManager::RegisterOnEntryPointEligibilityChanged(
    EntryPointEligibilityChangeCallbackList::CallbackType callback) {
  return entry_point_eligibility_change_callback_list_.Add(std::move(callback));
}

bool EntryPointEligibilityManager::IsSignedInToBrowserWithValidCredentials() {
  ContextualTasksUiService* const contextual_tasks_ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_);

  if (!contextual_tasks_ui_service) {
    return false;
  }
  return contextual_tasks_ui_service->IsSignedInToBrowserWithValidCredentials();
}

bool EntryPointEligibilityManager::CookieJarContainsPrimaryAccount() {
  ContextualTasksUiService* const contextual_tasks_ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_);

  if (!contextual_tasks_ui_service) {
    return false;
  }
  return contextual_tasks_ui_service->CookieJarContainsPrimaryAccount();
}

bool EntryPointEligibilityManager::AreEntryPointsEligible() {
  const bool is_signed_in_to_browser =
      IsSignedInToBrowserWithValidCredentials();
  const bool cookie_jar_contains_primary_account =
      CookieJarContainsPrimaryAccount();

  ContextualTasksService* const contextual_task_service =
      ContextualTasksServiceFactory::GetForProfile(profile_);
  CHECK(contextual_task_service);
  const bool is_feature_eligible =
      contextual_task_service->GetFeatureEligibility().IsEligible();
  const bool is_aim_allowed_by_policy =
      AimEligibilityService::IsAimAllowedByPolicy(profile_->GetPrefs());

  return is_signed_in_to_browser && cookie_jar_contains_primary_account &&
         is_feature_eligible && is_aim_allowed_by_policy;
}

void EntryPointEligibilityManager::MaybeNotifyEntryPointEligibilityChanged() {
  const bool updated_eligibility = AreEntryPointsEligible();
  if (entry_points_are_eligible_ != updated_eligibility) {
    entry_points_are_eligible_ = updated_eligibility;
    entry_point_eligibility_change_callback_list_.Notify(
        entry_points_are_eligible_);
  }
}
}  // namespace contextual_tasks
