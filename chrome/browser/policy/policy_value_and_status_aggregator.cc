// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_value_and_status_aggregator.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/browser/webui/policy_webui_constants.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_logger.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"
#include "chrome/browser/policy/status_provider/device_local_account_policy_status_provider.h"
#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider_chromeos.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/webui/machine_level_user_cloud_policy_status_provider.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/policy/status_provider/ash_lacros_policy_stack_bridge.h"
#include "chrome/browser/policy/status_provider/user_policy_status_provider_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/policy/status_provider/updater_status_and_value_provider.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/policy/value_provider/extension_policies_value_provider.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {
void AppendPolicyIdsToList(const base::Value::Dict& policy_values,
                           base::Value::List& policy_ids) {
  for (const auto id_policy_pair : policy_values) {
    policy_ids.Append(id_policy_pair.first);
  }
}

// Appends the ID of `policy_values` to `policy_ids` and merges it to
// `out_policy_values`.
void MergePolicyValuesAndIds(base::Value::Dict policy_values,
                             base::Value::Dict& out_policy_values,
                             base::Value::List& out_policy_ids) {
  AppendPolicyIdsToList(policy_values, out_policy_ids);
  out_policy_values.Merge(std::move(policy_values));
}

// Returns the PolicyStatusProvider for user policies for the current platform.
std::unique_ptr<policy::PolicyStatusProvider> GetUserPolicyStatusProvider(
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  policy::DeviceLocalAccountPolicyService* local_account_service =
      user_manager->IsLoggedInAsManagedGuestSession()
          ? connector->GetDeviceLocalAccountPolicyService()
          : nullptr;
  policy::UserCloudPolicyManagerAsh* user_cloud_policy =
      profile->GetUserCloudPolicyManagerAsh();
  if (local_account_service) {
    return std::make_unique<DeviceLocalAccountPolicyStatusProvider>(
        user_manager->GetActiveUser()->GetAccountId().GetUserEmail(),
        local_account_service);
  } else if (user_cloud_policy) {
    return std::make_unique<UserCloudPolicyStatusProviderChromeOS>(
        user_cloud_policy->core(), profile);
  }
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
  policy::CloudPolicyManager* cloud_policy_manager =
      profile->GetCloudPolicyManager();
  if (cloud_policy_manager) {
    return std::make_unique<UserCloudPolicyStatusProvider>(
        cloud_policy_manager->core(), profile);
  } else {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (profile->IsMainProfile()) {
      return std::make_unique<UserPolicyStatusProviderLacros>(
          g_browser_process->browser_policy_connector()
              ->device_account_policy_loader(),
          profile);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<policy::PolicyStatusProvider>();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the PolicyStatusProvider for ChromeOS device policies.
std::unique_ptr<policy::PolicyStatusProvider>
GetChromeOSDevicePolicyStatusProvider(
    Profile* profile,
    policy::BrowserPolicyConnectorAsh* connector) {
  return std::make_unique<DeviceCloudPolicyStatusProviderChromeOS>(connector);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Returns policy status provider for machine policies for non-ChromeOS
// platforms.
std::unique_ptr<policy::PolicyStatusProvider> GetMachinePolicyStatusProvider(
    policy::MachineLevelUserCloudPolicyManager* manager) {
  policy::BrowserDMTokenStorage* dmTokenStorage =
      policy::BrowserDMTokenStorage::Get();

  return std::make_unique<policy::MachineLevelUserCloudPolicyStatusProvider>(
      manager->core(), g_browser_process->local_state(),
      new policy::MachineLevelUserCloudPolicyContext(
          {dmTokenStorage->RetrieveEnrollmentToken(),
           dmTokenStorage->RetrieveClientId(),
           enterprise_reporting::kLastUploadSucceededTimestamp}));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace policy {

const char kUserStatusKey[] = "user";

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
const char kDeviceStatusKey[] = "device";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kMachineStatusKey[] = "machine";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kUpdaterStatusKey[] = "updater";
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

std::unique_ptr<PolicyValueAndStatusAggregator>
PolicyValueAndStatusAggregator::CreateDefaultPolicyValueAndStatusAggregator(
    Profile* profile) {
  std::unique_ptr<PolicyValueAndStatusAggregator> aggregator =
      base::WrapUnique(new PolicyValueAndStatusAggregator());

  // Add PolicyValueProviders.
  aggregator->AddPolicyValueProvider(
      std::make_unique<ChromePoliciesValueProvider>(profile));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  aggregator->AddPolicyValueProvider(
      std::make_unique<ExtensionPoliciesValueProvider>(profile));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Add PolicyStatusProviders.
  // User policies.
  aggregator->AddPolicyStatusProvider(kUserStatusKey,
                                      GetUserPolicyStatusProvider(profile));

  // Device policies.
  if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    aggregator->AddPolicyStatusProvider(
        kDeviceStatusKey, GetChromeOSDevicePolicyStatusProvider(
                              profile, g_browser_process->platform_part()
                                           ->browser_policy_connector_ash()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    aggregator->AddPolicyStatusAndValueProvider(
        kDeviceStatusKey, std::make_unique<AshLacrosPolicyStackBridge>());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  // Machine policies.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (manager) {
    policy::BrowserDMTokenStorage* dmTokenStorage =
        policy::BrowserDMTokenStorage::Get();
    LOG_POLICY(INFO, POLICY_PROCESSING)
        << "Retrieved Enrollment Token = "
        << dmTokenStorage->RetrieveEnrollmentToken()
        << " and client ID = " << dmTokenStorage->RetrieveClientId();

    aggregator->AddPolicyStatusProvider(
        kMachineStatusKey, GetMachinePolicyStatusProvider(manager));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

    // Updater policies.
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  aggregator->AddPolicyStatusAndValueProvider(
      kUpdaterStatusKey,
      std::make_unique<UpdaterStatusAndValueProvider>(profile));
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return aggregator;
}

PolicyValueAndStatusAggregator::PolicyValueAndStatusAggregator() = default;
PolicyValueAndStatusAggregator::~PolicyValueAndStatusAggregator() = default;

base::Value::Dict PolicyValueAndStatusAggregator::GetAggregatedPolicyStatus() {
  base::Value::Dict status;
  for (auto& status_provider_description_pair : status_providers_) {
    DVLOG_POLICY(3, POLICY_PROCESSING)
        << status_provider_description_pair.first
        << " status: " << status_provider_description_pair.second->GetStatus();
    status.Set(status_provider_description_pair.first,
               status_provider_description_pair.second->GetStatus());
  }
  return status;
}

base::Value::Dict PolicyValueAndStatusAggregator::GetAggregatedPolicyValues() {
  base::Value::Dict policy_values;
  base::Value::List policy_ids;
  for (auto& value_provider : value_providers_) {
    MergePolicyValuesAndIds(value_provider->GetValues(), policy_values,
                            policy_ids);
  }

  for (policy::PolicyValueProvider* value_provider : value_providers_unowned_) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // We only merge policy values for Lacros since policy ID is the same as
    // Chrome policies.
    policy_values.Merge(value_provider->GetValues());
#else
    MergePolicyValuesAndIds(value_provider->GetValues(), policy_values,
                            policy_ids);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
  base::Value::Dict dict;
  dict.Set(kPolicyValuesKey, std::move(policy_values));
  dict.Set(kPolicyIdsKey, std::move(policy_ids));
  return dict;
}

base::Value::Dict PolicyValueAndStatusAggregator::GetAggregatedPolicyNames() {
  base::Value::Dict policy_names;
  for (auto& value_provider : value_providers_) {
    policy_names.Merge(value_provider->GetNames());
  }
  for (policy::PolicyValueProvider* value_provider : value_providers_unowned_) {
    policy_names.Merge(value_provider->GetNames());
  }
  return policy_names;
}

void PolicyValueAndStatusAggregator::Refresh() {
  for (policy::PolicyValueProvider* value_provider : value_providers_unowned_) {
    value_provider->Refresh();
  }
  for (auto& value_provider : value_providers_) {
    value_provider->Refresh();
  }
}

void PolicyValueAndStatusAggregator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PolicyValueAndStatusAggregator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PolicyValueAndStatusAggregator::OnPolicyValueChanged() {
  NotifyValueAndStatusChange();
}

void PolicyValueAndStatusAggregator::OnPolicyStatusChanged() {
  NotifyValueAndStatusChange();
}

void PolicyValueAndStatusAggregator::NotifyValueAndStatusChange() {
  for (auto& observer : observers_)
    observer.OnPolicyValueAndStatusChanged();
}

void PolicyValueAndStatusAggregator::AddPolicyValueProvider(
    std::unique_ptr<PolicyValueProvider> value_provider) {
  policy_value_provider_observations_.AddObservation(value_provider.get());
  value_providers_.push_back(std::move(value_provider));
}

void PolicyValueAndStatusAggregator::AddPolicyStatusProvider(
    std::string status_provider_description,
    std::unique_ptr<PolicyStatusProvider> status_provider) {
  policy_status_provider_observations_.AddObservation(status_provider.get());
  status_providers_.emplace(status_provider_description,
                            std::move(status_provider));
}

void PolicyValueAndStatusAggregator::AddPolicyValueProviderUnowned(
    PolicyValueProvider* value_provider) {
  policy_value_provider_observations_.AddObservation(value_provider);
  value_providers_unowned_.push_back(value_provider);
}

}  // namespace policy
