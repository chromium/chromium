// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/restricted_mgs_policy_provider.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/settings/cros_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {

bool IsDeviceRestrictedManagedGuestSessionEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool device_restricted_managed_guest_session_enabled = false;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceRestrictedManagedGuestSessionEnabled,
      &device_restricted_managed_guest_session_enabled);
  return device_restricted_managed_guest_session_enabled;
#else
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  return init_params->DeviceSettings()
             ->device_restricted_managed_guest_session_enabled ==
         crosapi::mojom::DeviceSettings::OptionalBool::kTrue;
#endif
}

}  // namespace

namespace policy {

RestrictedMGSPolicyProvider::RestrictedMGSPolicyProvider() {
  UpdatePolicyBundle();
}

RestrictedMGSPolicyProvider::~RestrictedMGSPolicyProvider() = default;

// static
std::unique_ptr<RestrictedMGSPolicyProvider>
RestrictedMGSPolicyProvider::Create() {
  if (!chromeos::IsManagedGuestSession()) {
    return nullptr;
  }
  std::unique_ptr<RestrictedMGSPolicyProvider> provider(
      new RestrictedMGSPolicyProvider());
  return provider;
}

void RestrictedMGSPolicyProvider::RefreshPolicies(PolicyFetchReason reason) {}

void RestrictedMGSPolicyProvider::UpdatePolicyBundle() {
  weak_factory_.InvalidateWeakPtrs();
  PolicyBundle bundle = policies().Clone();

  PolicyMap& chrome_policy =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  if (IsDeviceRestrictedManagedGuestSessionEnabled())
    ApplyRestrictedManagedGuestSessionOverride(&chrome_policy);

  UpdatePolicy(std::move(bundle));
}

// Details about the restricted managed guest session and the overridden
// policies can be found here: go/restricted-managed-guest-session.
void RestrictedMGSPolicyProvider::ApplyRestrictedManagedGuestSessionOverride(
    PolicyMap* chrome_policy) {
  std::pair<std::string, base::Value> policy_overrides[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {key::kArcEnabled, base::Value(false)},
    {key::kCrostiniAllowed, base::Value(false)},
    {key::kDeletePrintJobHistoryAllowed, base::Value(true)},
    {key::kKerberosEnabled, base::Value(false)},
    {key::kNetworkFileSharesAllowed, base::Value(false)},
    {key::kUserBorealisAllowed, base::Value(false)},
    {key::kUserPluginVmAllowed, base::Value(false)},
#endif
    {key::kAllowDeletingBrowserHistory, base::Value(true)},
    {key::kCACertificateManagementAllowed,
     base::Value(static_cast<int>(CACertificateManagementPermission::kNone))},
    {key::kClientCertificateManagementAllowed,
     base::Value(
         static_cast<int>(ClientCertificateManagementPermission::kNone))},
    {key::kEnableMediaRouter, base::Value(false)},
    {key::kPasswordManagerEnabled, base::Value(false)},
    {key::kScreenCaptureAllowed, base::Value(false)},
  };

  for (auto& policy_override : policy_overrides) {
    chrome_policy->Set(policy_override.first, POLICY_LEVEL_MANDATORY,
                       POLICY_SCOPE_USER,
                       POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
                       std::move(policy_override.second), nullptr);
  }
}

}  // namespace policy
