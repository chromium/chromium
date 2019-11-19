// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"

namespace policy {

class PolicyMap;

// Policy provider for a device-local account. Pulls policy from
// DeviceLocalAccountPolicyService. Note that this implementation keeps
// functioning when the device-local account disappears from
// DeviceLocalAccountPolicyService. The current policy will be kept in that case
// and RefreshPolicies becomes a no-op. Policies for any installed extensions
// will be kept as well in that case.
class DeviceLocalAccountPolicyProvider
    : public ConfigurationPolicyProvider,
      public DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyProvider(
      const std::string& user_id,
      DeviceLocalAccountPolicyService* service,
      std::unique_ptr<PolicyMap> chrome_policy_overrides);
  ~DeviceLocalAccountPolicyProvider() override;

  // Factory function to create and initialize a provider for |user_id|. Returns
  // NULL if |user_id| is not a device-local account or user policy isn't
  // applicable for user_id's user type.
  // If |force_immediate_load| is true then policy is loaded synchronously on
  // creation.
  static std::unique_ptr<DeviceLocalAccountPolicyProvider> Create(
      const std::string& user_id,
      DeviceLocalAccountPolicyService* service,
      bool force_immediate_load);

  // ConfigurationPolicyProvider:
  bool IsInitializationComplete(PolicyDomain domain) const override;
  void RefreshPolicies() override;

  // DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

 private:
  // Returns the broker for |user_id_| or NULL if not available.
  DeviceLocalAccountPolicyBroker* GetBroker() const;

  // Handles completion of policy refreshes and triggers the update callback.
  // |success| is true if the policy refresh was successful.
  void ReportPolicyRefresh(bool success);

  // Unless |waiting_for_policy_refresh_|, calls UpdatePolicy(), using the
  // policy from the broker if available or keeping the current policy.
  void UpdateFromBroker();

  const std::string user_id_;
  scoped_refptr<DeviceLocalAccountExternalDataManager> external_data_manager_;

  DeviceLocalAccountPolicyService* service_;

  // A policy map providing overrides to apply on top of the Chrome policy
  // received from |service_|. This is used to fix certain policies for public
  // sessions regardless of what's actually specified in policy.
  std::unique_ptr<PolicyMap> chrome_policy_overrides_;

  bool store_initialized_;
  bool waiting_for_policy_refresh_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_
