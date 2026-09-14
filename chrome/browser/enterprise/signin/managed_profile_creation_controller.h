// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_CREATION_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_CREATION_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/identity_manager/account_info.h"

class DiceSignedInProfileCreator;
class DiceInterceptedSessionStartupHelper;
class Profile;
class ProfileAttributesEntry;

namespace signin {
class IdentityManager;
}  // namespace signin

enum class ManagedProfileCreationFailureReason {
  kNoActiveBrowser,
  kProfileCreationFailed,
  kNewProfileWasDeleted,
  kSourceProfileDeleted,
  kPrimaryAccountNotSet
};

using ManagedProfileCreationControllerCallback = base::OnceCallback<
    void(base::expected<Profile*, ManagedProfileCreationFailureReason>, bool)>;

// Used to create a managed profile for a specified account. This class is
// responsible for showing the enterprise management disclaimer dialog with
// pre-fetched profile separation policies and either converting the current
// profile into a managed profile, creating a new separate profile, or signing
// out depending on the user's explicit choice on the disclaimer.
class ManagedProfileCreationController : public ProfileObserver {
 public:
  ~ManagedProfileCreationController() override;

  ManagedProfileCreationController(const ManagedProfileCreationController&) =
      delete;
  ManagedProfileCreationController& operator=(
      const ManagedProfileCreationController&) = delete;

  // `source_profile` is the profile that is used as the basis for the sign-in.
  // It is also the profile that is converted into a managed profile or signed
  // out if the user chooses the corresponding options on the management
  // disclaimer.
  // `account_info` is the account that is being used to create the managed
  // profile.
  // `callback` is called when the profile creation is finished. It is called
  // with nullptr if the profile creation is cancelled by the user. If the
  // profile creation is canceled for any other reason, it is called with an
  // error. Otherwise, it is called with the created or the converted profile
  // and a boolean value that indicates whether the profile creation was
  // required by policy.
  // `profile_separation_policies` contains the profile separation policies for
  // the account.
  [[nodiscard]] static std::unique_ptr<ManagedProfileCreationController>
  CreateManagedProfile(
      Profile* source_profile,
      const AccountInfo& account_info,
      signin_metrics::AccessPoint access_point,
      ManagedProfileCreationControllerCallback callback,
      policy::ProfileSeparationPolicies profile_separation_policies);

  [[nodiscard]] static std::unique_ptr<ManagedProfileCreationController>
  CreateManagedProfileForTesting(
      Profile* source_profile,
      const AccountInfo& account_info,
      signin_metrics::AccessPoint access_point,
      ManagedProfileCreationControllerCallback callback,
      std::optional<policy::ProfileSeparationPolicies>
          profile_separation_policies = std::nullopt,
      std::optional<signin::SigninChoice> user_choice = std::nullopt);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  ManagedProfileCreationController(
      Profile* source_profile,
      const AccountInfo& account_info,
      signin_metrics::AccessPoint access_point,
      ManagedProfileCreationControllerCallback callback,
      policy::ProfileSeparationPolicies profile_separation_policies);

  bool Init();

  void ShowManagementDisclaimer();
  void OnManagementDisclaimerResult(signin::SigninChoice choice);

  void ConvertSourceProfileIntoManagedProfile();
  void Signout();
  void MoveAccountIntoNewProfile();
  void OnNewSignedInProfileCreated(bool is_new_profile, Profile* new_profile);
  void OnNewBrowserCreated();

  void OnProfileCreationDone(
      base::expected<Profile*, ManagedProfileCreationFailureReason> result);

  signin::IdentityManager* GetIdentityManager();

  // Profile where the signin is initiated.
  raw_ptr<Profile> source_profile_ = nullptr;
  raw_ptr<Profile> final_profile_ = nullptr;
  const AccountInfo account_info_;
  const signin_metrics::AccessPoint access_point_;
  const policy::ProfileSeparationPolicies profile_separation_policies_;
  std::optional<signin::SigninChoice> user_choice_for_testing_;
  bool skip_browser_startup_for_testing_ = false;
  bool profile_creation_required_by_policy_ = false;
  bool allows_converting_profile_to_managed_ = true;
  ManagedProfileCreationControllerCallback callback_;
  std::unique_ptr<DiceSignedInProfileCreator> profile_creator_;
  std::unique_ptr<DiceInterceptedSessionStartupHelper> startup_helper_;
  base::ScopedObservation<Profile, ProfileObserver> source_profile_observation_{
      this};
  base::ScopedObservation<Profile, ProfileObserver> final_profile_observation_{
      this};

  base::WeakPtrFactory<ManagedProfileCreationController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_CREATION_CONTROLLER_H_
