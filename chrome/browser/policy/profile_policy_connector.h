// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#else
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif

namespace user_manager {
class User;
}

namespace policy {
namespace internal {
#if BUILDFLAG(IS_CHROMEOS_ASH)
class ProxiedPoliciesPropagatedWatcher;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
class LocalTestInfoBarVisibilityManager;
}  // namespace internal

class CloudPolicyStore;
class ConfigurationPolicyProvider;
class PolicyMigrator;
class PolicyServiceImpl;
class SchemaRegistry;
class ChromeBrowserPolicyConnector;

// The ProfilePolicyConnector creates and manages the per-Profile policy
// components. Since the ProfilePolicyConnector instance is accessed from
// Profile, not from a KeyedServiceFactory anymore, the ProfilePolicyConnector
// no longer needs to be a KeyedService.
class ProfilePolicyConnector final : public PolicyService::Observer {
 public:
  ProfilePolicyConnector();
  ProfilePolicyConnector(const ProfilePolicyConnector&) = delete;
  ProfilePolicyConnector& operator=(const ProfilePolicyConnector&) = delete;
  ~ProfilePolicyConnector() override;

  // |user| is only used in Chrome OS builds and should be set to nullptr
  // otherwise.  |configuration_policy_provider| and |policy_store| are nullptr
  // for non-regular users.
  // If |force_immediate_load| is true, DeviceLocalAccountPolicy is loaded
  // synchronously.
  void Init(const user_manager::User* user,
            SchemaRegistry* schema_registry,
            ConfigurationPolicyProvider* configuration_policy_provider,
            const CloudPolicyStore* policy_store,
            policy::ChromeBrowserPolicyConnector* browser_policy_connector,
            bool force_immediate_load);

  void InitForTesting(std::unique_ptr<PolicyService> service);
  void OverrideIsManagedForTesting(bool is_managed);

  void Shutdown();

  // This is never NULL.
  PolicyService* policy_service() const { return policy_service_.get(); }

  // Returns true if this Profile is under any kind of policy management. You
  // must call this method only when the policies system is fully initialized.
  bool IsManaged() const;

  // Returns true if the |policy_key| user policy is currently set via the
  // |configuration_policy_provider_| and isn't being overridden by a
  // higher-level provider.
  bool IsProfilePolicy(const char* policy_key) const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Triggers the time out handling of waiting for the proxied primary user
  // policies to propagate. May be only called form tests.
  void TriggerProxiedPoliciesWaitTimeoutForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Returns affiliation IDs contained in the PolicyData corresponding to the
  // profile.
  base::flat_set<std::string> user_affiliation_ids() const;
  void SetUserAffiliationIdsForTesting(
      const base::flat_set<std::string>& user_affiliation_ids);

  // PolicyService::Observer:
  void OnPolicyServiceInitialized(PolicyDomain domain) override;

  // Sets the local_test_policy_provider as active and all other policy
  // providers to inactive.
  void UseLocalTestPolicyProvider();

  // Reverts the effects of UseLocalTestPolicyProvider.
  void RevertUseLocalTestPolicyProvider();

  // Returns true if policies from chrome://policy/test are applied.
  bool IsUsingLocalTestPolicyProvider() const;

 private:
  void DoPostInit();

  // Returns the policy store which is actually used.
  const CloudPolicyStore* GetActualPolicyStore() const;

  // Find the policy provider that provides the |policy_key| policy, if any. In
  // case of multiple providers sharing the same policy, the one with the
  // highest priority will be returned.
  const ConfigurationPolicyProvider* DeterminePolicyProviderForPolicy(
      const char* policy_key) const;

  void AppendPolicyProviderWithSchemaTracking(
      ConfigurationPolicyProvider* policy_provider,
      SchemaRegistry* schema_registry);

  std::string GetTimeToFirstPolicyLoadMetricSuffix() const;

  // Records profile affiliation-related metrics and then starts a 7 day timer
  // with itself as the callback. This ensures metrics are recorded at least
  // every 7 days if the profile remains open.
  void RecordAffiliationMetrics();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, primary Profile user policies are forwarded to the
  // device-global PolicyService[1] using a ProxyPolicyProvider.
  // When that is done, signaling that |policy_service_| is initialized should
  // be delayed until the policies provided by |user_policy_delegate| have
  // propagated to the device-wide PolicyService[1]. This is done so that code
  // that runs early on Profile initialization can rely on the device-wide
  // PolicyService[1] and local state Preferences[2] respecting the proxied
  // primary user policies.
  //
  // This function starts watching for the propagation to happen by creating a
  // |ProxiedPoliciesPropagatedWatcher| and creates a PolicyService that
  // only signals that it is initilalized when the
  // |proxied_policies_propagated_watcher_| has fired.
  //
  // [1] i.e. g_browser_process->policy_service()
  // [2] i.e. g_browser_process->local_state()
  std::unique_ptr<PolicyService> CreatePolicyServiceWithInitializationThrottled(
      const std::vector<raw_ptr<ConfigurationPolicyProvider,
                                VectorExperimental>>& policy_providers,
      std::vector<std::unique_ptr<PolicyMigrator>> migrators,
      ConfigurationPolicyProvider* user_policy_delegate);

  // Called when primary user policies that are proxied to the device-wide
  // PolicyService have propagated.
  // |policy_service| is passed here because UnthrottleInitialization is
  // implemented on PolicyServiceImpl, but the |policy_service_| class member is
  // a PolicyService for testability.
  void OnProxiedPoliciesPropagated(PolicyServiceImpl* policy_service);

  raw_ptr<const user_manager::User, DanglingUntriaged> user_ = nullptr;

  // Some of the user policy configuration affects browser global state, and
  // can only come from one Profile. |is_primary_user_| is true if this
  // connector belongs to the first signed-in Profile, and in that case that
  // Profile's policy is the one that affects global policy settings in
  // local state.
  bool is_primary_user_ = false;
  // Whether the user was freshly created in this session.
  bool is_user_new_ = false;

  std::unique_ptr<ConfigurationPolicyProvider> special_user_policy_provider_;

  // If the user associated with the Profile for this ProfilePolicyConnector is
  // the primary user, the user policies will be proxied into the device-wide
  // PolicyService. This object allows calling a callback when that is finished
  // so it is possible to delay signaling that |policy_service_| is initialized
  // until the policies have been reflected in the device-wide PolicyService.
  std::unique_ptr<internal::ProxiedPoliciesPropagatedWatcher>
      proxied_policies_propagated_watcher_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Wrap policy provider with SchemaRegistryTrackingPolicyProvider to track
  // extensions' policy schema update.
  std::vector<std::unique_ptr<ConfigurationPolicyProvider>>
      wrapped_policy_providers_;

  raw_ptr<const ConfigurationPolicyProvider> configuration_policy_provider_ =
      nullptr;
  raw_ptr<const CloudPolicyStore> policy_store_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ConfigurationPolicyProvider> restricted_mgs_policy_provider;
#endif

  // |policy_providers_| contains a list of the policy providers available for
  // the PolicyService of this connector, in decreasing order of priority.
  //
  // Note: All the providers appended to this vector must eventually become
  // initialized for every policy domain, otherwise some subsystems will never
  // use the policies exposed by the PolicyService!
  // The default ConfigurationPolicyProvider::IsInitializationComplete()
  // result is true, so take care if a provider overrides that.
  std::vector<raw_ptr<ConfigurationPolicyProvider, VectorExperimental>>
      policy_providers_;

  std::unique_ptr<PolicyService> policy_service_;

  std::unique_ptr<bool> is_managed_override_;

  raw_ptr<ConfigurationPolicyProvider> local_test_policy_provider_ = nullptr;

  std::unique_ptr<internal::LocalTestInfoBarVisibilityManager>
      local_test_infobar_visibility_manager_;

  base::RetainingOneShotTimer management_status_metrics_timer_;

  base::flat_set<std::string> user_affiliation_ids_for_testing_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns |true| when this is the main profile.
  bool IsMainProfile() const;

  // The |browser_policy_connector_| is owned by the |BrowserProcess| whereas
  // the |ProfilePolicyConnector| is owned by the Profile - which gets deleted
  // first - so the lifetime of the pointer is guaranteed.
  raw_ptr<ChromeBrowserPolicyConnector> browser_policy_connector_ = nullptr;
#endif
};
}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
