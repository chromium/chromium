// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/external_data/device_local_account_external_data_manager.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

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
  DeviceLocalAccountPolicyProvider(const std::string& user_id,
                                   DeviceLocalAccountPolicyService* service,
                                   DeviceLocalAccountType type);

  DeviceLocalAccountPolicyProvider(const DeviceLocalAccountPolicyProvider&) =
      delete;
  DeviceLocalAccountPolicyProvider& operator=(
      const DeviceLocalAccountPolicyProvider&) = delete;

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
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;
  void RefreshPolicies(PolicyFetchReason reason) override;

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

  raw_ptr<DeviceLocalAccountPolicyService> service_;
  DeviceLocalAccountType type_;

  bool store_initialized_;
  bool waiting_for_policy_refresh_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyProvider> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_PROVIDER_H_
