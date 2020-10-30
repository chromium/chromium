// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/component_active_directory_policy_service.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_scheduler.h"

namespace chromeos {

class AuthPolicyHelper;

}  // namespace chromeos

namespace policy {

class CloudExternalDataManager;

// ConfigurationPolicyProvider for policy from Active Directory.
// Derived classes implement specializations for user and device policy.
// Data flow: Triggered by DoPolicyFetch(), policy is fetched by authpolicyd and
// stored in session manager with completion indicated by OnPolicyFetched().
// From there policy load from session manager is triggered, completion of which
// is notified via OnStoreLoaded()/OnStoreError().
class ActiveDirectoryPolicyManager
    : public ConfigurationPolicyProvider,
      public CloudPolicyStore::Observer,
      public ComponentActiveDirectoryPolicyService::Delegate {
 public:
  ~ActiveDirectoryPolicyManager() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  void RefreshPolicies() override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;
  void OnStoreError(CloudPolicyStore* cloud_policy_store) override;

  // ComponentActiveDirectoryPolicyService::Delegate:
  void OnComponentActiveDirectoryPolicyUpdated() override;

  CloudPolicyStore* store() const { return store_.get(); }
  CloudExternalDataManager* external_data_manager() {
    return external_data_manager_.get();
  }
  PolicyScheduler* scheduler() { return scheduler_.get(); }
  ComponentActiveDirectoryPolicyService* extension_policy_service() {
    return extension_policy_service_.get();
  }

 protected:
  ActiveDirectoryPolicyManager(
      std::unique_ptr<CloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      PolicyDomain extension_policy_domain);

  // Publish the policy that's currently cached in the store.
  void PublishPolicy();

  // Creates the policy service to load extension policy from Session Manager.
  // |scope| specifies whether the component policy is fetched along with user
  // or device policy. |account_type| specifies which account Session Manager
  // should load policy from (device vs user). |account_id| must be empty for
  // the device account and the user's account id for user accounts.
  // |schema_registry| is the registry that contains the extension schemas.
  void CreateExtensionPolicyService(
      PolicyScope scope,
      login_manager::PolicyAccountType account_type,
      const AccountId& account_id,
      SchemaRegistry* schema_registry);

  // Calls into authpolicyd to fetch policy. Reports success or failure via
  // |callback|.
  virtual void DoPolicyFetch(PolicyScheduler::TaskCallback callback) = 0;

  // Allows derived classes to cancel waiting for the initial policy fetch/load
  // and to flag the ConfigurationPolicyProvider ready (assuming all other
  // initialization tasks have completed) or to exit the session in case the
  // requirements to continue have not been met.
  virtual void CancelWaitForInitialPolicy() {}

  // Called by PublishPolicy() before the policy is sent off to UpdatePolicy().
  virtual void OnPublishPolicy() {}

  // Whether policy fetch has ever been reported as completed by authpolicyd
  // during lifetime of the object (after Chrome was started).
  bool fetch_ever_completed_ = false;

  chromeos::AuthPolicyHelper* authpolicy_helper() const {
    return authpolicy_helper_.get();
  }

 private:
  // Called by scheduler with result of policy fetch. This covers policy
  // download, parsing and storing into session manager. (To access and publish
  // the policy, the store needs to be reloaded from session manager.)
  void OnPolicyFetched(bool success);

  // Called right before policy is published. Expands e.g. ${MACHINE_NAME} for
  // a selected set of policies.
  void ExpandVariables(PolicyMap* policy_map);

  // Store used to serialize policy, usually sends data to Session Manager.
  const std::unique_ptr<CloudPolicyStore> store_;

  // Manages external data referenced by policies.
  const std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  // Manages policy for Chrome extensions.
  std::unique_ptr<ComponentActiveDirectoryPolicyService>
      extension_policy_service_;

  // Type of extension policy to manage. Must be either POLICY_DOMAIN_EXTENSIONS
  // or POLICY_DOMAIN_SIGNIN_EXTENSIONS.
  const PolicyDomain extension_policy_domain_;

  std::unique_ptr<PolicyScheduler> scheduler_;

  std::unique_ptr<chromeos::AuthPolicyHelper> authpolicy_helper_;

  // Must be last member.
  base::WeakPtrFactory<ActiveDirectoryPolicyManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryPolicyManager);
};

// Manages user policy for Active Directory managed devices.
class UserActiveDirectoryPolicyManager : public ActiveDirectoryPolicyManager {
 public:
  // If |initial_policy_fetch_timeout| is non-zero, IsInitializationComplete()
  // is forced to false until either there has been a successful policy fetch
  // from the server and a subsequent successful load from session manager or
  // |initial_policy_fetch_timeout| has expired and there has been a successful
  // load from session manager. If |policy_required| is true then the user
  // session is aborted by calling |exit_session| if no policy was loaded from
  // session manager and this is either immediate load in case of Chrome restart
  // or policy fetch failed.
  UserActiveDirectoryPolicyManager(
      const AccountId& account_id,
      bool policy_required,
      base::TimeDelta initial_policy_fetch_timeout,
      base::OnceClosure exit_session,
      std::unique_ptr<CloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager);

  ~UserActiveDirectoryPolicyManager() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  bool IsInitializationComplete(PolicyDomain domain) const override;

  // Helper function to force a policy fetch timeout.
  void ForceTimeoutForTesting();

 protected:
  // ActiveDirectoryPolicyManager:

  // Calls AuthPolicyClient to fetch user policy.
  void DoPolicyFetch(PolicyScheduler::TaskCallback callback) override;

  // Cancels the initial wait timeout for policy fetches during sign-in.
  void CancelWaitForInitialPolicy() override;

  // Updates user affiliation IDs.
  void OnPublishPolicy() override;

 private:
  // Called when |initial_policy_timeout_| times out, to cancel the blocking
  // wait for the initial policy fetch.
  void OnBlockingFetchTimeout();

  // The user's account id.
  const AccountId account_id_;

  // If policy is required, but cannot be obtained (via fetch or load),
  // |exit_session_| is called.
  const bool policy_required_;

  // Whether we're waiting for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool waiting_for_initial_policy_fetch_ = false;

  // A timer that puts a hard limit on the maximum time to wait for the initial
  // policy fetch/load.
  base::OneShotTimer initial_policy_timeout_;

  // Callback to exit the session.
  base::OnceClosure exit_session_;

  // Must be last member.
  base::WeakPtrFactory<UserActiveDirectoryPolicyManager> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(UserActiveDirectoryPolicyManager);
};

// Manages device policy for Active Directory managed devices.
class DeviceActiveDirectoryPolicyManager : public ActiveDirectoryPolicyManager {
 public:
  explicit DeviceActiveDirectoryPolicyManager(
      std::unique_ptr<CloudPolicyStore> store);
  ~DeviceActiveDirectoryPolicyManager() override;

  // ConfigurationPolicyProvider:
  void Shutdown() override;

  // Passes the |schema_registry| that corresponds to the signin profile and
  // uses it (wrapped in a ForwardingSchemaRegistry) to create the extension
  // policy service.
  void SetSigninProfileSchemaRegistry(SchemaRegistry* schema_registry);

 protected:
  // ActiveDirectoryPolicyManager:

  // Calls AuthPolicyClient to fetch device policy.
  void DoPolicyFetch(PolicyScheduler::TaskCallback callback) override;

 private:
  // Wrapper schema registry that tracks the signin profile schema registry once
  // it is passed to this class.
  std::unique_ptr<ForwardingSchemaRegistry>
      signin_profile_forwarding_schema_registry_;

  DISALLOW_COPY_AND_ASSIGN(DeviceActiveDirectoryPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_POLICY_MANAGER_H_
