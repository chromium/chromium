// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_policy_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/external_data/device_local_account_external_data_manager.h"
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
    DeviceLocalAccountType type)
    : user_id_(user_id),
      service_(service),
      type_(type),
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
  auto type = GetDeviceLocalAccountType(user_id);
  if (!device_local_account_policy_service || !type.has_value()) {
    return nullptr;
  }

  auto provider = std::make_unique<DeviceLocalAccountPolicyProvider>(
      user_id, device_local_account_policy_service, type.value());
  // In case of restore-after-restart broker should already be initialized.
  if (force_immediate_load && provider->GetBroker())
    provider->GetBroker()->LoadImmediately();
  return provider;
}

bool DeviceLocalAccountPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_initialized_;
  if (ComponentCloudPolicyService::SupportsDomain(domain) && GetBroker() &&
      GetBroker()->component_policy_service()) {
    return GetBroker()->component_policy_service()->is_initialized();
  }
  return true;
}

bool DeviceLocalAccountPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return IsInitializationComplete(domain);
}

void DeviceLocalAccountPolicyProvider::RefreshPolicies(
    PolicyFetchReason reason) {
  DeviceLocalAccountPolicyBroker* broker = GetBroker();
  if (broker && broker->core()->service()) {
    waiting_for_policy_refresh_ = true;
    broker->core()->service()->RefreshPolicy(
        base::BindOnce(&DeviceLocalAccountPolicyProvider::ReportPolicyRefresh,
                       weak_factory_.GetWeakPtr()),
        reason);
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
  PolicyBundle bundle;
  if (broker) {
    store_initialized_ |= broker->core()->store()->is_initialized();
    if (!waiting_for_policy_refresh_) {
      // Copy policy from the broker.
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
          broker->core()->store()->policy_map().Clone();
      external_data_manager_ = broker->external_data_manager();

      if (broker->component_policy_service())
        bundle.MergeFrom(broker->component_policy_service()->policy());
    } else {
      // Wait for the refresh to finish.
      return;
    }
  } else {
    // Keep existing policy, but do send an update.
    waiting_for_policy_refresh_ = false;
    weak_factory_.InvalidateWeakPtrs();
    bundle = policies().Clone();
  }

  PolicyMap& chrome_policy =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  // Apply the defaults for policies that haven't been configured by the
  // administrator given that this is an enterprise user.
  SetEnterpriseUsersDefaults(&chrome_policy);

  switch (type_) {
    case DeviceLocalAccountType::kPublicSession: {
      // Apply managed guest session specific default values if no value is
      // fetched from the cloud.

      if (!chrome_policy.IsPolicySet(key::kShelfAutoHideBehavior)) {
        // Force the |ShelfAutoHideBehavior| policy to |Never|, ensuring that
        // the ash shelf does not auto-hide.
        chrome_policy.Set(key::kShelfAutoHideBehavior, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_ENTERPRISE_DEFAULT,
                          base::Value("Never"), nullptr);
      }

      if (!chrome_policy.IsPolicySet(key::kShowLogoutButtonInTray)) {
        // Force the |ShowLogoutButtonInTray| policy to |true|, ensuring that a
        // big, red logout button is shown in the ash system tray.
        chrome_policy.Set(key::kShowLogoutButtonInTray, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(true),
                          nullptr);
      }
      break;
    }

    case DeviceLocalAccountType::kWebKioskApp:
    case DeviceLocalAccountType::kKioskIsolatedWebApp:
      // Disable translation functionality in Web Kiosk Mode.
      chrome_policy.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
                        POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                        base::Value(false), nullptr);
      break;
    case DeviceLocalAccountType::kSamlPublicSession:
    case DeviceLocalAccountType::kKioskApp:
      // Do nothing.
      break;
  }

  UpdatePolicy(std::move(bundle));
}

}  // namespace policy
