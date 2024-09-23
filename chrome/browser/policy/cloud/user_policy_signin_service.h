// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class AccountId;
class Profile;
class ProfileManager;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class UserPolicySigninService;

// Observer bridge for UserPolicySigninService to observe profile manager
// events.
class ProfileManagerObserverBridge : public ProfileManagerObserver {
 public:
  explicit ProfileManagerObserverBridge(
      UserPolicySigninService* user_policy_signin_service);
  ProfileManagerObserverBridge(const ProfileManagerObserverBridge&) = delete;
  ProfileManagerObserverBridge& operator=(const ProfileManagerObserverBridge&) =
      delete;
  ~ProfileManagerObserverBridge() override;

  // ProfileManagerObserver implementation:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  raw_ptr<UserPolicySigninService> user_policy_signin_service_;
};

// A specialization of the UserPolicySigninServiceBase for the desktop
// platforms (Windows, Mac and Linux).
class UserPolicySigninService : public UserPolicySigninServiceBase,
                                public ProfileAttributesStorage::Observer,
                                public signin::IdentityManager::Observer {
 public:
  // Creates a UserPolicySigninService associated with the passed
  // |policy_manager| and |identity_manager|.
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

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // UserPolicySigninServiceBase implementation:
  void ShutdownCloudPolicyManager() override;

  // ProfileAttributesStorage::Observer implementation:
  void OnProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) override;

  // Handler for when the profile is ready.
  void OnProfileReady(Profile* profile);

  // Called when the ProfileAttributesStorage is being destroyed.
  void OnProfileAttributesStorageDestroying();

  void set_profile_can_be_managed_for_testing(bool can_be_managed) {
    profile_can_be_managed_for_testing_ = can_be_managed;
  }

  // KeyedService implementation:
  void Shutdown() override;

 private:
  // UserPolicySigninServiceBase implementation:
  void InitializeCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client) override;
  void ProhibitSignoutIfNeeded() override;
  bool CanApplyPolicies(bool check_for_refresh_token) override;
  CloudPolicyClient::DeviceDMTokenCallback
  GetDeviceDMTokenIfAffiliatedCallback() override;
  std::string GetProfileId() override;

  // Helper method that attempts calls |InitializeForSignedInUser| only if
  // |policy_manager| is not-nul. Expects that there is a refresh token for
  // the primary account.
  void TryInitializeForSignedInUser();

  // Initializes the UserPolicySigninService once its owning Profile becomes
  // ready. If the Profile has a signed-in account associated with it at startup
  // then this initializes the cloud policy manager by calling
  // InitializeForSignedInUser(); otherwise it clears any stored policies.
  void InitializeOnProfileReady(Profile* profile);

  // True when the profile can be managed for testing purpose. Has to be set
  // from the test fixture. This is used to bypass the check on the profile
  // attributes entry.
  bool profile_can_be_managed_for_testing_ = false;

  // Parent profile for this service.
  raw_ptr<Profile> profile_;

  // Observer bridge for profile added events.
  ProfileManagerObserverBridge profile_manager_observer_bridge_{this};

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      observed_profile_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
