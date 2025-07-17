// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_MANAGEMENT_DISCLAIMER_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_MANAGEMENT_DISCLAIMER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/signin/managed_profile_creation_controller.h"
#include "chrome/browser/enterprise/signin/managed_profile_creator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;
class ProfileAttributesEntry;

namespace signin {
class IdentityManager;
}

// Service responsible to show enterprise management disclaimers at startup on
// the focused browser to profiles where the signed in account is a managed
// account and the user has yet to accept profile management.
class ProfileManagementDisclaimerService
    : public KeyedService,
      public signin::IdentityManager::Observer,
      public BrowserListObserver {
 public:
  explicit ProfileManagementDisclaimerService(Profile* profile);
  ~ProfileManagementDisclaimerService() override;

  ProfileManagementDisclaimerService(
      const ProfileManagementDisclaimerService&) = delete;
  ProfileManagementDisclaimerService& operator=(
      const ProfileManagementDisclaimerService&) = delete;

  // The callback will be called with the profile that was chosen to be managed,
  // it may be null if the user denied management.
  // See `ManagedProfileCreationController::CreateManagedProfile()` for a
  // detailed description of the `callback` parameter.
  // The caller must ensure that we are not already creating a managed profile
  // for another account using `GetAccountBeingConsideredForManagementIfAny()`.
  void EnsureManagedProfileForAccount(
      const CoreAccountId& account_id,
      signin_metrics::AccessPoint access_point,
      base::OnceCallback<void(Profile*, bool)> callback);

  // Returns an empty `CoreAccountId` if no profile creation is in progress.
  const CoreAccountId& GetAccountBeingConsideredForManagementIfAny() const;

  void SetProfileSeparationPoliciesForTesting(
      std::optional<policy::ProfileSeparationPolicies> value) {
    profile_separation_policies_for_testing_ = std::move(value);
  }

  void SetUserChoiceForTesting(signin::SigninChoice choice) {
    user_choice_for_testing_ = std::move(choice);
  }

  base::ScopedClosureRunner DisableManagementDisclaimerUntilReset();

 private:
  struct ResetableState {
    ResetableState();
    ~ResetableState();
    ResetableState(const ResetableState& other) = delete;
    ResetableState& operator=(const ResetableState& other) = delete;

    // Timeout for waiting for full information to be available.
    base::OneShotTimer extended_account_info_wait_timeout;

    std::unique_ptr<ManagedProfileCreationController>
        profile_creation_controller;

    signin_metrics::AccessPoint access_point =
        signin_metrics::AccessPoint::kUnknown;
    base::WeakPtr<Profile> profile_to_continue_in;
    CoreAccountId account_id;
    bool profile_creation_required_by_policy = false;

    // Callbacks to be executed the user chooses which profile to be managed and
    // whether management is required by policy. The first parameter is the
    // profile that was chosen, it may be null if the user denied management.
    // The second parameter is whether management is required by policy.
    base::OnceCallbackList<void(Profile*, bool)> callbacks;
  };

  signin::IdentityManager* GetIdentityManager();
  AccountInfo GetPrimaryAccountInfo();
  AccountInfo GetExtendedAccountInfo(const CoreAccountId& account_id);

  // Attempts to show the enterprise management disclaimer. The disclaimer will
  // be shown if the following conditions are met:
  // - There is a browser opened with focus for this profile.
  // - There is a primary account with all the info available and it is managed.
  // - The user has not yet accepted the management disclaimer.
  // - No disclaimer is currently shown.
  // - There is no Signin interception happening.
  // - The profile separation policies for the primary account are known.
  // If either information is unknown, we will attempt to fetch it and retry to
  // show the disclaimer once we receive it.
  void MaybeShowEnterpriseManagementDisclaimer(
      const CoreAccountId& account_id,
      signin_metrics::AccessPoint access_point);

  void OnManagedProfileCreationResult(
      base::expected<Profile*, ManagedProfileCreationFailureReason> result,
      bool profile_creation_required_by_policy);

  // Resets the state of this service after a management disclaimer has been
  // handled.
  void Reset();

  void SetEnableManagementDisclaimerOnPrimaryAccountChange(bool enabled) {
    enable_management_disclaimer_ = enabled;
  }

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  const raw_ref<Profile> profile_;
  std::unique_ptr<ResetableState> state_;
  std::optional<policy::ProfileSeparationPolicies>
      profile_separation_policies_for_testing_;
  std::optional<signin::SigninChoice> user_choice_for_testing_;

  bool enable_management_disclaimer_ = true;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  base::ScopedObservation<BrowserList, BrowserListObserver>
      scoped_browser_list_observation_{this};

  base::WeakPtrFactory<ProfileManagementDisclaimerService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_MANAGEMENT_DISCLAIMER_SERVICE_H_
