// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

DeviceLocalAccountPolicyProvider::DeviceLocalAccountPolicyProvider(
    const std::string& user_id,
    DeviceLocalAccountPolicyService* service,
    std::unique_ptr<PolicyMap> chrome_policy_overrides)
    : user_id_(user_id),
      service_(service),
      chrome_policy_overrides_(std::move(chrome_policy_overrides)),
      store_initialized_(false),
      waiting_for_policy_refresh_(false) {
  service_->AddObserver(this);
  UpdateFromBroker();
}

DeviceLocalAccountPolicyProvider::~DeviceLocalAccountPolicyProvider() {
  service_->RemoveObserver(this);
}

// static
std::unique_ptr<DeviceLocalAccountPolicyProvider>
DeviceLocalAccountPolicyProvider::Create(
    const std::string& user_id,
    DeviceLocalAccountPolicyService* device_local_account_policy_service,
    bool force_immediate_load) {
  DeviceLocalAccount::Type type;
  if (!device_local_account_policy_service ||
      !IsDeviceLocalAccountUser(user_id, &type)) {
    return nullptr;
  }

  std::unique_ptr<PolicyMap> chrome_policy_overrides;
  if (type == DeviceLocalAccount::TYPE_PUBLIC_SESSION) {
    chrome_policy_overrides = std::make_unique<PolicyMap>();

    // Force the |ShelfAutoHideBehavior| policy to |Never|, ensuring that the
    // ash shelf does not auto-hide.
    chrome_policy_overrides->Set(key::kShelfAutoHideBehavior,
                                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                 POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,
                                 base::Value("Never"), nullptr);
    // Force the |ShowLogoutButtonInTray| policy to |true|, ensuring that a big,
    // red logout button is shown in the ash system tray.
    chrome_policy_overrides->Set(key::kShowLogoutButtonInTray,
                                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                 POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,
                                 base::Value(true), nullptr);
  }

  std::unique_ptr<DeviceLocalAccountPolicyProvider> provider(
      new DeviceLocalAccountPolicyProvider(user_id,
                                           device_local_account_policy_service,
                                           std::move(chrome_policy_overrides)));
  // In case of restore-after-restart broker should already be initialized.
  if (force_immediate_load && provider->GetBroker())
    provider->GetBroker()->LoadImmediately();
  return provider;
}

bool DeviceLocalAccountPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_initialized_;
  if (ComponentCloudPolicyService::SupportsDomain(domain) &&
      GetBroker() && GetBroker()->component_policy_service()) {
    return GetBroker()->component_policy_service()->is_initialized();
  }
  return true;
}

bool DeviceLocalAccountPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return IsInitializationComplete(domain);
}

void DeviceLocalAccountPolicyProvider::RefreshPolicies() {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  if (broker && broker->core()->service()) {
    waiting_for_policy_refresh_ = true;
    broker->core()->service()->RefreshPolicy(
        base::BindOnce(&DeviceLocalAccountPolicyProvider::ReportPolicyRefresh,
                       weak_factory_.GetWeakPtr()));
  } else {
    UpdateFromBroker();
  }
}

void DeviceLocalAccountPolicyProvider::OnPolicyUpdated(
    const std::string& user_id) {
  if (user_id == user_id_)
    UpdateFromBroker();
}

void DeviceLocalAccountPolicyProvider::OnDeviceLocalAccountsChanged() {
  UpdateFromBroker();
}

DeviceLocalAccountPolicyBroker* DeviceLocalAccountPolicyProvider::GetBroker()
    const {
  return service_->GetBrokerForUser(user_id_);
}

void DeviceLocalAccountPolicyProvider::ReportPolicyRefresh(bool success) {
  waiting_for_policy_refresh_ = false;
  UpdateFromBroker();
}

void DeviceLocalAccountPolicyProvider::UpdateFromBroker() {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
  if (broker) {
    store_initialized_ |= broker->core()->store()->is_initialized();
    if (!waiting_for_policy_refresh_) {
      // Copy policy from the broker.
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .CopyFrom(broker->core()->store()->policy_map());
      external_data_manager_ = broker->external_data_manager();

      if (broker->component_policy_service())
        bundle->MergeFrom(broker->component_policy_service()->policy());
    } else {
      // Wait for the refresh to finish.
      return;
    }
  } else {
    // Keep existing policy, but do send an update.
    waiting_for_policy_refresh_ = false;
    weak_factory_.InvalidateWeakPtrs();
    bundle->CopyFrom(policies());
  }

  PolicyMap& chrome_policy =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  // Apply the defaults for policies that haven't been configured by the
  // administrator given that this is an enterprise user.
  SetEnterpriseUsersDefaults(&chrome_policy);

  // Apply overrides.
  if (chrome_policy_overrides_) {
    for (const auto& policy_override : *chrome_policy_overrides_) {
      const PolicyMap::Entry& entry = policy_override.second;
      chrome_policy.Set(policy_override.first, entry.level, entry.scope,
                        entry.source, entry.value()->Clone(), nullptr);
    }
  }

  UpdatePolicy(std::move(bundle));
}

}  // namespace policy
