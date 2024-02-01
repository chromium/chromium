// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_MOBILE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_MOBILE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

// A specialization of UserPolicySigninServiceBase for Android.
class UserPolicySigninService : public UserPolicySigninServiceBase,
                                public ProfileManagerObserver,
                                public signin::IdentityManager::Observer {
 public:
  // Creates a UserPolicySigninService associated with the passed |profile|.
  UserPolicySigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  UserPolicySigninService(const UserPolicySigninService&) = delete;
  UserPolicySigninService& operator=(const UserPolicySigninService&) = delete;
  ~UserPolicySigninService() override;

  // Overridden from UserPolicySigninServiceForProfile to cancel the pending
  // delayed registration.
  void ShutdownCloudPolicyManager() override;

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // ProfileManagerObserver implementation.
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  void set_profile_can_be_managed_for_testing(bool can_be_managed) {
    profile_can_be_managed_for_testing_ = can_be_managed;
  }

 private:
  // KeyedService implementation:
  void Shutdown() override;

  // UserPolicySigninServiceBase implementation:
  base::TimeDelta GetTryRegistrationDelay() override;
  void UpdateLastPolicyCheckTime() override;
  signin::ConsentLevel GetConsentLevelForRegistration() override;
  bool CanApplyPolicies(bool check_for_refresh_token) override;
  void InitializeCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client) override;
  CloudPolicyClient::DeviceDMTokenCallback
  GetDeviceDMTokenIfAffiliatedCallback() override;
  std::string GetProfileId() override;

  // Initializes the UserPolicySigninService once its owning Profile becomes
  // ready. If the Profile has a signed-in account associated with it at startup
  // then this initializes the cloud policy manager by calling
  // InitializeForSignedInUser(); otherwise it clears any stored policies.
  void InitializeOnProfileReady(Profile* profile);

  // True when the profile can be managed for testing purpose. Has to be set
  // from the test fixture. This is used to bypass the check on the profile
  // attributes entry.
  bool profile_can_be_managed_for_testing_ = false;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  // The PrefService associated with the profile.
  raw_ptr<PrefService> profile_prefs_;

  // Parent profile for this service.
  raw_ptr<Profile> profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_MOBILE_H_
