// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_policy_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/external_data/device_local_account_external_data_manager.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
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
    DeviceLocalAccount::Type type)
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
  DeviceLocalAccount::Type type;
  if (!device_local_account_policy_service ||
      !IsDeviceLocalAccountUser(user_id, &type)) {
    return nullptr;
  }

  std::unique_ptr<DeviceLocalAccountPolicyProvider> provider(
      new DeviceLocalAccountPolicyProvider(
          user_id, device_local_account_policy_service, type));
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
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
          broker->core()->store()->policy_map().Clone();
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

  // Apply managed guest session specific default values if no value is fetched
  // from the cloud.
  if (type_ == DeviceLocalAccount::TYPE_PUBLIC_SESSION) {
    if (chrome_policy.GetValueUnsafe(key::kShelfAutoHideBehavior) == nullptr) {
      // Force the |ShelfAutoHideBehavior| policy to |Never|, ensuring that the
      // ash shelf does not auto-hide.
      chrome_policy.Set(key::kShelfAutoHideBehavior, POLICY_LEVEL_MANDATORY,
                        POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                        base::Value("Never"), nullptr);
    }

    if (chrome_policy.GetValueUnsafe(key::kShowLogoutButtonInTray) == nullptr) {
      // Force the |ShowLogoutButtonInTray| policy to |true|, ensuring that a
      // big, red logout button is shown in the ash system tray.
      chrome_policy.Set(key::kShowLogoutButtonInTray, POLICY_LEVEL_MANDATORY,
                        POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                        base::Value(true), nullptr);
    }
  }

  // Disable translation functionality in Web Kiosk Mode.
  if (type_ == DeviceLocalAccount::TYPE_WEB_KIOSK_APP) {
    chrome_policy.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                      base::Value(false), nullptr);
  }

  bool device_restricted_managed_guest_session_enabled = false;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceRestrictedManagedGuestSessionEnabled,
      &device_restricted_managed_guest_session_enabled);
  if (device_restricted_managed_guest_session_enabled) {
    ApplyRestrictedManagedGuestSessionOverride(&chrome_policy);
  }

  UpdatePolicy(std::move(bundle));
}

// Details about the restricted managed guest session and the overridden
// policies can be found here: go/restricted-managed-guest-session.
void DeviceLocalAccountPolicyProvider::
    ApplyRestrictedManagedGuestSessionOverride(PolicyMap* chrome_policy) {
  std::pair<std::string, base::Value> policy_overrides[] = {
      {key::kPasswordManagerEnabled, base::Value(false)},
      {key::kAllowDeletingBrowserHistory, base::Value(true)},
      {key::kArcEnabled, base::Value(false)},
      {key::kCrostiniAllowed, base::Value(false)},
      {key::kUserPluginVmAllowed, base::Value(false)},
      {key::kNetworkFileSharesAllowed, base::Value(false)},
      {key::kCACertificateManagementAllowed,
       base::Value(static_cast<int>(CACertificateManagementPermission::kNone))},
      {key::kClientCertificateManagementAllowed,
       base::Value(
           static_cast<int>(ClientCertificateManagementPermission::kNone))},
      {key::kEnableMediaRouter, base::Value(false)},
      {key::kScreenCaptureAllowed, base::Value(false)},
      {key::kKerberosEnabled, base::Value(false)},
      {key::kUserBorealisAllowed, base::Value(false)},
      {key::kDeletePrintJobHistoryAllowed, base::Value(true)},
      {key::kLacrosSecondaryProfilesAllowed, base::Value(false)},
      {key::kLacrosAvailability, base::Value("lacros_disallowed")},
  };

  for (auto& policy_override : policy_overrides) {
    chrome_policy->Set(policy_override.first, POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER,
                       POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                       std::move(policy_override.second), nullptr);
  }
}

}  // namespace policy
