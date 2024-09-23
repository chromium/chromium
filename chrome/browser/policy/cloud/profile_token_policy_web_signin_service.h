// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_PROFILE_TOKEN_POLICY_WEB_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_PROFILE_TOKEN_POLICY_WEB_SIGNIN_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/signin/profile_token_web_signin_interceptor.h"
#include "chrome/browser/policy/client_data_delegate_desktop.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_helper.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"

class AccountId;
class PrefService;
class Profile;
class ProfileManager;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudPolicyClient;
class DeviceManagementService;
class ProfileCloudPolicyManager;

class ProfileTokenPolicyWebSigninService : public UserPolicySigninServiceBase {
 public:
  // Observer bridge for ProfileTokenPolicyWebSigninService to observe profile
  // manager events.
  class ProfileManagerObserverBridge : public ProfileManagerObserver {
   public:
    explicit ProfileManagerObserverBridge(
        ProfileTokenPolicyWebSigninService*
            profile_token_signin_policy_service);
    ProfileManagerObserverBridge(const ProfileManagerObserverBridge&) = delete;
    ProfileManagerObserverBridge& operator=(
        const ProfileManagerObserverBridge&) = delete;
    ~ProfileManagerObserverBridge() override;

    // ProfileManagerObserver implementation:
    void OnProfileAdded(Profile* profile) override;
    void OnProfileManagerDestroying() override;

   private:
    base::ScopedObservation<ProfileManager, ProfileManagerObserver>
        profile_manager_observation_{this};
    raw_ptr<ProfileTokenPolicyWebSigninService>
        profile_token_signin_policy_service_;
  };

  ProfileTokenPolicyWebSigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      ProfileCloudPolicyManager* policy_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  ProfileTokenPolicyWebSigninService(
      const ProfileTokenPolicyWebSigninService&) = delete;
  ProfileTokenPolicyWebSigninService& operator=(
      const ProfileTokenPolicyWebSigninService&) = delete;

  ~ProfileTokenPolicyWebSigninService() override;

  // Handler for when the profile is ready.
  void OnProfileReady(Profile* profile);

  // UserPolicySigninServiceBase implementation:
  void ShutdownCloudPolicyManager() override;

  // CloudPolicyService::Observer implementation:
  std::string_view name() const override;

 protected:
  // UserPolicySigninServiceBase implementation:
  void RegisterCloudPolicyService() override;

 private:
  void OnRegistrationComplete(const std::string& dm_token,
                              const std::string& client_id);
  void OnPolicyFetchComplete(bool success);

  // UserPolicySigninServiceBase implementation:
  void InitializeCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client) override;
  bool CanApplyPolicies(bool check_for_refresh_token) override;
  std::string GetProfileId() override;

  // Initializes the UserPolicySigninService once its owning Profile becomes
  // ready. If the Profile has a signed-in account associated with it at startup
  // then this initializes the cloud policy manager by calling
  // InitializeForSignedInUser(); otherwise it clears any stored policies.
  void InitializeOnProfileReady(Profile* profile);

  // Parent profile for this service.
  raw_ptr<Profile> profile_;
  ClientDataDelegateDesktop client_data_delegate_;
  std::unique_ptr<ChromeBrowserCloudManagementRegistrar>
      cloud_policy_registrar_;

  // Observer bridge for profile added events.
  ProfileManagerObserverBridge profile_manager_observer_bridge_{this};

  base::WeakPtrFactory<ProfileTokenPolicyWebSigninService> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_PROFILE_TOKEN_POLICY_WEB_SIGNIN_SERVICE_H_
