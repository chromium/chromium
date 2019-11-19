// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema_registry_tracking_policy_provider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"
#include "components/policy/core/common/proxy_policy_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace policy {

#if defined(OS_CHROMEOS)
namespace internal {

// This class allows observing a |device_wide_policy_service| for policy updates
// during which the |source_policy_provider| has already been initialized.
// It is used to know when propagation of primary user policies proxied to the
// device-wide PolicyService has finished.
class ProxiedPoliciesPropagatedWatcher : PolicyService::ProviderUpdateObserver {
 public:
  ProxiedPoliciesPropagatedWatcher(
      PolicyService* device_wide_policy_service,
      ProxyPolicyProvider* proxy_policy_provider,
      ConfigurationPolicyProvider* source_policy_provider,
      base::OnceClosure proxied_policies_propagated_callback)
      : device_wide_policy_service_(device_wide_policy_service),
        proxy_policy_provider_(proxy_policy_provider),
        source_policy_provider_(source_policy_provider),
        proxied_policies_propagated_callback_(
            std::move(proxied_policies_propagated_callback)) {
    device_wide_policy_service->AddProviderUpdateObserver(this);

    timeout_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(
            kProxiedPoliciesPropagationTimeoutInSeconds),
        this,
        &ProxiedPoliciesPropagatedWatcher::OnProviderUpdatePropagationTimedOut);
  }

  ~ProxiedPoliciesPropagatedWatcher() override {
    device_wide_policy_service_->RemoveProviderUpdateObserver(this);
  }

  // PolicyService::Observer:
  void OnProviderUpdatePropagated(
      ConfigurationPolicyProvider* provider) override {
    if (!proxied_policies_propagated_callback_)
      return;
    if (provider != proxy_policy_provider_)
      return;

    if (!source_policy_provider_->IsInitializationComplete(
            POLICY_DOMAIN_CHROME)) {
      return;
    }

    std::move(proxied_policies_propagated_callback_).Run();
  }

  void OnProviderUpdatePropagationTimedOut() {
    if (!proxied_policies_propagated_callback_)
      return;
    LOG(WARNING) << "Waiting for proxied policies to propagate timed out.";
    std::move(proxied_policies_propagated_callback_).Run();
  }

 private:
  static constexpr int kProxiedPoliciesPropagationTimeoutInSeconds = 5;

  PolicyService* const device_wide_policy_service_;
  const ProxyPolicyProvider* const proxy_policy_provider_;
  const ConfigurationPolicyProvider* const source_policy_provider_;
  base::OnceClosure proxied_policies_propagated_callback_;
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ProxiedPoliciesPropagatedWatcher);
};

}  // namespace internal

namespace {
// Returns the PolicyService that holds device-wide policies.
PolicyService* GetDeviceWidePolicyService() {
  BrowserPolicyConnectorChromeOS* browser_policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return browser_policy_connector->GetPolicyService();
}

// Returns the ProxyPolicyProvider which is used to forward primary Profile
// policies into the device-wide PolicyService.
ProxyPolicyProvider* GetProxyPolicyProvider() {
  BrowserPolicyConnectorChromeOS* browser_policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return browser_policy_connector->GetGlobalUserCloudPolicyProvider();
}
}  // namespace

#endif  // defined(OS_CHROMEOS)

ProfilePolicyConnector::ProfilePolicyConnector() {}

ProfilePolicyConnector::~ProfilePolicyConnector() {}

void ProfilePolicyConnector::Init(
    const user_manager::User* user,
    SchemaRegistry* schema_registry,
    ConfigurationPolicyProvider* configuration_policy_provider,
    const CloudPolicyStore* policy_store,
    policy::ChromeBrowserPolicyConnector* connector,
    bool force_immediate_load) {
  configuration_policy_provider_ = configuration_policy_provider;
  policy_store_ = policy_store;

#if defined(OS_CHROMEOS)
  auto* browser_policy_connector =
      static_cast<BrowserPolicyConnectorChromeOS*>(connector);
#else
  DCHECK_EQ(nullptr, user);
#endif

  ConfigurationPolicyProvider* platform_provider =
      GetPlatformProvider(connector);
  if (platform_provider) {
    wrapped_platform_policy_provider_ =
        std::make_unique<SchemaRegistryTrackingPolicyProvider>(
            platform_provider);
    wrapped_platform_policy_provider_->Init(schema_registry);
    policy_providers_.push_back(wrapped_platform_policy_provider_.get());
  }

#if defined(OS_CHROMEOS)
  if (browser_policy_connector->GetDeviceCloudPolicyManager()) {
    policy_providers_.push_back(
        browser_policy_connector->GetDeviceCloudPolicyManager());
  }
  if (browser_policy_connector->GetDeviceActiveDirectoryPolicyManager()) {
    policy_providers_.push_back(
        browser_policy_connector->GetDeviceActiveDirectoryPolicyManager());
  }
#else
  for (auto* provider : connector->GetPolicyProviders()) {
    // Skip the platform provider since it was already handled above.  The
    // platform provider should be first in the list so that it always takes
    // precedence.
    if (provider == platform_provider) {
      continue;
    } else {
      // TODO(zmin): In the future, we may want to have special handling for
      // the other providers too.
      policy_providers_.push_back(provider);
    }
  }
#endif

  if (configuration_policy_provider)
    policy_providers_.push_back(configuration_policy_provider);

#if defined(OS_CHROMEOS)
  if (!user) {
    DCHECK(schema_registry);
    // This case occurs for the signin and the lock screen app profiles.
    special_user_policy_provider_.reset(new LoginProfilePolicyProvider(
        browser_policy_connector->GetPolicyService()));
  } else {
    // |user| should never be nullptr except for the signin and the lock screen
    // app profile.
    is_primary_user_ =
        user == user_manager::UserManager::Get()->GetPrimaryUser();
    // Note that |DeviceLocalAccountPolicyProvider::Create| returns nullptr when
    // the user supplied is not a device-local account user.
    special_user_policy_provider_ = DeviceLocalAccountPolicyProvider::Create(
        user->GetAccountId().GetUserEmail(),
        browser_policy_connector->GetDeviceLocalAccountPolicyService(),
        force_immediate_load);
  }
  if (special_user_policy_provider_) {
    special_user_policy_provider_->Init(schema_registry);
    policy_providers_.push_back(special_user_policy_provider_.get());
  }
#endif

#if defined(OS_CHROMEOS)
  ConfigurationPolicyProvider* user_policy_delegate_candidate =
      configuration_policy_provider ? configuration_policy_provider
                                    : special_user_policy_provider_.get();

  // Only proxy primary user policies to the device_wide policy service if all
  // of the following are true:
  // (*) This ProfilePolicyConnector has been created for the primary user.
  // (*) There is a policy provider for this profile. Note that for unmanaged
  //     users, |user_policy_delegate_candidate| will be nullptr.
  // (*) The ProxyPolicyProvider is actually used by the device-wide policy
  //     service. This may not be the case  e.g. in tests that use
  //     bBrowserPolicyConnectorBase::SetPolicyProviderForTesting.
  if (is_primary_user_ && user_policy_delegate_candidate &&
      GetDeviceWidePolicyService()->HasProvider(GetProxyPolicyProvider())) {
    GetProxyPolicyProvider()->SetDelegate(user_policy_delegate_candidate);

    // When proxying primary user policies to the device-wide PolicyService,
    // delay signaling that initialization is complete until the policies have
    // propagated. See CreatePolicyServiceWithInitializationThrottled for
    // details.
    policy_service_ = CreatePolicyServiceWithInitializationThrottled(
        policy_providers_, user_policy_delegate_candidate);
  } else {
    policy_service_ = std::make_unique<PolicyServiceImpl>(policy_providers_);
  }
#else   // defined(OS_CHROMEOS)
  policy_service_ = std::make_unique<PolicyServiceImpl>(policy_providers_);
#endif  // defined(OS_CHROMEOS)
}

void ProfilePolicyConnector::InitForTesting(
    std::unique_ptr<PolicyService> service) {
  DCHECK(!policy_service_);
  policy_service_ = std::move(service);
}

void ProfilePolicyConnector::OverrideIsManagedForTesting(bool is_managed) {
  is_managed_override_.reset(new bool(is_managed));
}

void ProfilePolicyConnector::SetPlatformPolicyProviderForTesting(
    ConfigurationPolicyProvider* platform_policy_provider_for_testing) {
  platform_policy_provider_for_testing_ = platform_policy_provider_for_testing;
}

void ProfilePolicyConnector::Shutdown() {
#if defined(OS_CHROMEOS)
  if (is_primary_user_)
    GetProxyPolicyProvider()->SetDelegate(nullptr);

  if (special_user_policy_provider_)
    special_user_policy_provider_->Shutdown();
#endif

  if (wrapped_platform_policy_provider_)
    wrapped_platform_policy_provider_->Shutdown();
}

bool ProfilePolicyConnector::IsManaged() const {
  if (is_managed_override_)
    return *is_managed_override_;
  const CloudPolicyStore* actual_policy_store = GetActualPolicyStore();
  if (actual_policy_store)
    return actual_policy_store->is_managed();
  return false;
}

bool ProfilePolicyConnector::IsProfilePolicy(const char* policy_key) const {
  const ConfigurationPolicyProvider* const provider =
      DeterminePolicyProviderForPolicy(policy_key);
  return provider == configuration_policy_provider_;
}

#if defined(OS_CHROMEOS)
void ProfilePolicyConnector::TriggerProxiedPoliciesWaitTimeoutForTesting() {
  CHECK(proxied_policies_propagated_watcher_);
  proxied_policies_propagated_watcher_->OnProviderUpdatePropagationTimedOut();
}
#endif  // defined(OS_CHROMEOS)

const CloudPolicyStore* ProfilePolicyConnector::GetActualPolicyStore() const {
  if (policy_store_)
    return policy_store_;
#if defined(OS_CHROMEOS)
  if (special_user_policy_provider_) {
    // |special_user_policy_provider_| is non-null for device-local accounts,
    // for the login profile, and the lock screen app profile.
    const DeviceCloudPolicyManagerChromeOS* const device_cloud_policy_manager =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetDeviceCloudPolicyManager();
    // The device_cloud_policy_manager can be a nullptr in unit tests.
    if (device_cloud_policy_manager)
      return device_cloud_policy_manager->core()->store();
  }
#endif
  return nullptr;
}

const ConfigurationPolicyProvider*
ProfilePolicyConnector::DeterminePolicyProviderForPolicy(
    const char* policy_key) const {
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  for (const ConfigurationPolicyProvider* provider : policy_providers_) {
    if (provider->policies().Get(chrome_ns).Get(policy_key))
      return provider;
  }
  return nullptr;
}

ConfigurationPolicyProvider* ProfilePolicyConnector::GetPlatformProvider(
    policy::ChromeBrowserPolicyConnector* browser_policy_connector) {
  if (platform_policy_provider_for_testing_)
    return platform_policy_provider_for_testing_;
  return browser_policy_connector->GetPlatformProvider();
}

#if defined(OS_CHROMEOS)
std::unique_ptr<PolicyService>
ProfilePolicyConnector::CreatePolicyServiceWithInitializationThrottled(
    const std::vector<ConfigurationPolicyProvider*>& policy_providers,
    ConfigurationPolicyProvider* user_policy_delegate) {
  DCHECK(user_policy_delegate);

  auto policy_service =
      PolicyServiceImpl::CreateWithThrottledInitialization(policy_providers);

  // base::Unretained is OK for |this| because
  // |proxied_policies_propagated_watcher_| is guaranteed not to call its
  // callback after it has been destroyed. base::Unretained is also OK for
  // |policy_service.get()| because it will be owned by |*this| and is never
  // explicitly destroyed.
  proxied_policies_propagated_watcher_ =
      std::make_unique<internal::ProxiedPoliciesPropagatedWatcher>(
          GetDeviceWidePolicyService(), GetProxyPolicyProvider(),
          user_policy_delegate,
          base::BindOnce(&ProfilePolicyConnector::OnProxiedPoliciesPropagated,
                         base::Unretained(this),
                         base::Unretained(policy_service.get())));
  return std::move(policy_service);
}

void ProfilePolicyConnector::OnProxiedPoliciesPropagated(
    PolicyServiceImpl* policy_service) {
  policy_service->UnthrottleInitialization();
  // Do not delete |proxied_policies_propagated_watcher_| synchronously, as the
  // PolicyService it is observing is expected to be iterating its observer
  // list.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, std::move(proxied_policies_propagated_watcher_));
}
#endif

}  // namespace policy
