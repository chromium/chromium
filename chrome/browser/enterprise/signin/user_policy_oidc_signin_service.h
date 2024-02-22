// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_USER_POLICY_OIDC_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_USER_POLICY_OIDC_SIGNIN_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace policy {

class ProfileCloudPolicyManager;
class UserCloudPolicyManager;
class UserPolicyOidcSigninService;

// Observer bridge for UserPolicyOidcSigninService to observe profile manager
// events.
class OidcProfileManagerObserverBridge : public ProfileManagerObserver {
 public:
  explicit OidcProfileManagerObserverBridge(
      UserPolicyOidcSigninService* user_policy_signin_service);
  OidcProfileManagerObserverBridge(const OidcProfileManagerObserverBridge&) =
      delete;
  OidcProfileManagerObserverBridge& operator=(
      const OidcProfileManagerObserverBridge&) = delete;
  ~OidcProfileManagerObserverBridge() override;

  // ProfileManagerObserver implementation:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  raw_ptr<UserPolicyOidcSigninService> user_policy_signin_service_;
};

// A specialized UserPolicySigninServiceBase that manages policies only for
// Oidc-enrolled profiles. This sign in service uses user-level for dasher-based
// profiles and profile-level policy manager for dasherless profiles.
// This policy sign in service shares CloudPolicyManager with other policy sign
// in services.
class UserPolicyOidcSigninService : public UserPolicySigninServiceBase {
 public:
  UserPolicyOidcSigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
          policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  UserPolicyOidcSigninService(const UserPolicyOidcSigninService&) = delete;
  UserPolicyOidcSigninService& operator=(const UserPolicyOidcSigninService&) =
      delete;
  ~UserPolicyOidcSigninService() override;

  // Handler for when the profile is ready.
  void OnProfileReady(Profile* profile);

  // CloudPolicyService::Observer implementation:
  void OnCloudPolicyServiceInitializationCompleted() override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;

 private:
  // UserPolicySigninServiceBase implementation:
  void InitializeCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client) override;
  // UserPolicySigninServiceBase implementation:
  std::string GetProfileId() override;

  // UserPolicySigninServiceBase implementation:
  bool CanApplyPolicies(bool check_for_refresh_token) override;

  // Initializes the UserPolicyOidcSigninService once its owning Profile becomes
  // ready, only if the profile already has policies and is a dasherless
  // profile. For Dasher-based 3P profile, UserPolicySigninService will take
  // care of the initialization.
  void InitializeOnProfileReady(Profile* profile);

  // Parent profile for this service.
  raw_ptr<Profile> profile_;

  // Observer bridge for profile added events.
  OidcProfileManagerObserverBridge profile_manager_observer_bridge_{this};

  // Callbacks to invoke upon policy fetch.
  std::unique_ptr<base::OnceCallbackList<void(bool)>>
      oidc_policy_fetch_callbacks_;

  base::WeakPtrFactory<UserPolicyOidcSigninService> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_USER_POLICY_OIDC_SIGNIN_SERVICE_H_
