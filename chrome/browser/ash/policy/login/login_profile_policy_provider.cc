// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/login/login_profile_policy_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {
struct DevicePolicyToUserPolicyMapEntry {
  const char* const device_policy_name;
  const char* const user_policy_name;
};

const char kLidCloseAction[] = "LidCloseAction";
const char kUserActivityScreenDimDelayScale[] =
    "UserActivityScreenDimDelayScale";

const char kActionSuspend[] = "Suspend";
const char kActionLogout[] = "Logout";
const char kActionShutdown[] = "Shutdown";
const char kActionDoNothing[] = "DoNothing";

// All policies in this list should have a pref mapping test case in
// components/policy/test/data/pref_mapping/[PolicyName].json with location
// "signin_profile".
const DevicePolicyToUserPolicyMapEntry kDevicePoliciesWithPolicyOptionsMap[] = {
    {key::kDeviceLoginScreenAutoSelectCertificateForUrls,
     key::kAutoSelectCertificateForUrls},
    {key::kDeviceLoginScreenLargeCursorEnabled, key::kLargeCursorEnabled},
    {key::kDeviceLoginScreenSpokenFeedbackEnabled, key::kSpokenFeedbackEnabled},
    {key::kDeviceLoginScreenHighContrastEnabled, key::kHighContrastEnabled},
    {key::kDeviceLoginScreenVirtualKeyboardEnabled,
     key::kVirtualKeyboardEnabled},
    {key::kDeviceLoginScreenDictationEnabled, key::kDictationEnabled},
    {key::kDeviceLoginScreenSelectToSpeakEnabled, key::kSelectToSpeakEnabled},
    {key::kDeviceLoginScreenCursorHighlightEnabled,
     key::kCursorHighlightEnabled},
    {key::kDeviceLoginScreenCaretHighlightEnabled, key::kCaretHighlightEnabled},
    {key::kDeviceLoginScreenMonoAudioEnabled, key::kMonoAudioEnabled},
    {key::kDeviceLoginScreenAutoclickEnabled, key::kAutoclickEnabled},
    {key::kDeviceLoginScreenStickyKeysEnabled, key::kStickyKeysEnabled},
    {key::kDeviceLoginScreenKeyboardFocusHighlightEnabled,
     key::kKeyboardFocusHighlightEnabled},
    {key::kDeviceLoginScreenScreenMagnifierType, key::kScreenMagnifierType},
    {key::kDeviceLoginScreenShowOptionsInSystemTrayMenu,
     key::kShowAccessibilityOptionsInSystemTrayMenu},
    {key::kDeviceLoginScreenPrimaryMouseButtonSwitch,
     key::kPrimaryMouseButtonSwitch},
    {key::kDeviceLoginScreenAccessibilityShortcutsEnabled,
     key::kAccessibilityShortcutsEnabled},
    {key::kDeviceLoginScreenPrivacyScreenEnabled, key::kPrivacyScreenEnabled},
    {key::kDeviceLoginScreenWebUsbAllowDevicesForUrls,
     key::kWebUsbAllowDevicesForUrls},
    {key::kDeviceLoginScreenExtensions, key::kExtensionInstallForcelist},
    {key::kDeviceLoginScreenExtensionManifestV2Availability,
     key::kExtensionManifestV2Availability},
    {key::kDeviceLoginScreenPromptOnMultipleMatchingCertificates,
     key::kPromptOnMultipleMatchingCertificates},
    {key::kDeviceLoginScreenContextAwareAccessSignalsAllowlist,
     key::kUserContextAwareAccessSignalsAllowlist},
    {key::kDeviceLoginScreenTouchVirtualKeyboardEnabled,
     key::kTouchVirtualKeyboardEnabled},

    // The authentication URL blocklist and allowlist policies implement content
    // control for authentication flows, including in the login screen and lock
    // screen.  Since these use the SigninProfile and LockScreenProfile, content
    // control is already possible there through the URLBlocklist/URLAllowlist
    // user policies.
    {key::kDeviceAuthenticationURLBlocklist, key::kURLBlocklist},
    {key::kDeviceAuthenticationURLAllowlist, key::kURLAllowlist},

    // key::kDeviceLoginScreenLocales maps to the ash::kDeviceLoginScreenLocales
    // CrosSetting elsewhere. Also map it to the key::kForcedLanguages policy in
    // the login/lock screen profile so web contents within those profiles
    // generate a corresponding Accept-Languages header
    // (https://crbug.com/1336382).
    {key::kDeviceLoginScreenLocales, key::kForcedLanguages},
    {key::kDeviceScreensaverLoginScreenEnabled,
     key::kScreensaverLockScreenEnabled},
    {key::kDeviceScreensaverLoginScreenIdleTimeoutSeconds,
     key::kScreensaverLockScreenIdleTimeoutSeconds},
    {key::kDeviceScreensaverLoginScreenImageDisplayIntervalSeconds,
     key::kScreensaverLockScreenImageDisplayIntervalSeconds},
    {key::kDeviceScreensaverLoginScreenImages,
     key::kScreensaverLockScreenImages},
};

const DevicePolicyToUserPolicyMapEntry kRecommendedDevicePoliciesMap[] = {
    {key::kDeviceLoginScreenDefaultLargeCursorEnabled,
     key::kLargeCursorEnabled},
    {key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
     key::kSpokenFeedbackEnabled},
    {key::kDeviceLoginScreenDefaultHighContrastEnabled,
     key::kHighContrastEnabled},
    {key::kDeviceLoginScreenDefaultScreenMagnifierType,
     key::kScreenMagnifierType},
    {key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
     key::kVirtualKeyboardEnabled},
};

std::optional<base::Value> GetAction(const std::string& action) {
  if (action == kActionSuspend) {
    return base::Value(chromeos::PowerPolicyController::ACTION_SUSPEND);
  }
  if (action == kActionLogout) {
    return base::Value(chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  }
  if (action == kActionShutdown) {
    return base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN);
  }
  if (action == kActionDoNothing) {
    return base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING);
  }
  return std::nullopt;
}

// Applies |value| as the recommended value of |user_policy| in
// |user_policy_map|. If |value| is nullptr, does nothing.
void ApplyValueAsRecommendedPolicy(const base::Value* value,
                                   const std::string& user_policy,
                                   PolicyMap* user_policy_map) {
  if (value) {
    user_policy_map->Set(user_policy, POLICY_LEVEL_RECOMMENDED,
                         POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, value->Clone(),
                         nullptr);
  }
}

// Applies the value of |device_policy| in |device_policy_map| as the
// recommended value of |user_policy| in |user_policy_map|. If the value of
// |device_policy| is unset, does nothing.
void ApplyDevicePolicyAsRecommendedPolicy(const std::string& device_policy,
                                          const std::string& user_policy,
                                          const PolicyMap& device_policy_map,
                                          PolicyMap* user_policy_map) {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = device_policy_map.GetValueUnsafe(device_policy);
  ApplyValueAsRecommendedPolicy(value, user_policy, user_policy_map);
}

// Applies |value| as the mandatory value of |user_policy| in |user_policy_map|.
// If |value| is NULL, does nothing.
void ApplyValueAsMandatoryPolicy(const base::Value& value,
                                 const std::string& user_policy,
                                 PolicyMap* user_policy_map) {
  user_policy_map->Set(user_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_CLOUD, value.Clone(), nullptr);
}

void ApplyDevicePolicyWithPolicyOptions(const std::string& device_policy,
                                        const std::string& user_policy,
                                        const PolicyMap& device_policy_map,
                                        PolicyMap* user_policy_map) {
  const PolicyMap::Entry* entry = device_policy_map.Get(device_policy);
  if (entry) {
    user_policy_map->Set(user_policy, entry->level, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, entry->value_unsafe()->Clone(),
                         nullptr);
  }
}
}  // namespace

LoginProfilePolicyProvider::LoginProfilePolicyProvider(
    PolicyService* device_policy_service)
    : device_policy_service_(device_policy_service),
      waiting_for_device_policy_refresh_(false) {}

LoginProfilePolicyProvider::~LoginProfilePolicyProvider() {}

void LoginProfilePolicyProvider::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);
  device_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME))
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::Shutdown() {
  device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  weak_factory_.InvalidateWeakPtrs();
  ConfigurationPolicyProvider::Shutdown();
}

void LoginProfilePolicyProvider::RefreshPolicies(PolicyFetchReason reason) {
  waiting_for_device_policy_refresh_ = true;
  weak_factory_.InvalidateWeakPtrs();
  device_policy_service_->RefreshPolicies(
      base::BindOnce(&LoginProfilePolicyProvider::OnDevicePolicyRefreshDone,
                     weak_factory_.GetWeakPtr()),
      reason);
}

void LoginProfilePolicyProvider::OnPolicyUpdated(const PolicyNamespace& ns,
                                                 const PolicyMap& previous,
                                                 const PolicyMap& current) {
  if (ns == PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain == POLICY_DOMAIN_CHROME)
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::OnDevicePolicyRefreshDone() {
  waiting_for_device_policy_refresh_ = false;
  UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::UpdateFromDevicePolicy() {
  // If a policy refresh is in progress, wait for it to finish.
  if (waiting_for_device_policy_refresh_)
    return;

  const PolicyNamespace chrome_namespaces(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& device_policy_map =
      device_policy_service_->GetPolicies(chrome_namespaces);
  PolicyBundle bundle;
  PolicyMap& user_policy_map = bundle.Get(chrome_namespaces);

  // The device policies which includes the policy options
  // |kDevicePoliciesWithPolicyOptionsMap| should be applied after
  // |kRecommendedDevicePoliciesMap|, because its overrides some deprecated ones
  // there.
  for (const auto& entry : kRecommendedDevicePoliciesMap) {
    ApplyDevicePolicyAsRecommendedPolicy(entry.device_policy_name,
                                         entry.user_policy_name,
                                         device_policy_map, &user_policy_map);
  }

  for (const auto& entry : kDevicePoliciesWithPolicyOptionsMap) {
    ApplyDevicePolicyWithPolicyOptions(entry.device_policy_name,
                                       entry.user_policy_name,
                                       device_policy_map, &user_policy_map);
  }

  const base::Value* value = device_policy_map.GetValue(
      key::kDeviceLoginScreenPowerManagement, base::Value::Type::DICT);
  if (value) {
    base::Value::Dict policy_dict = value->GetDict().Clone();
    const std::string* lid_close_action =
        policy_dict.FindString(kLidCloseAction);

    if (lid_close_action) {
      std::optional<base::Value> action = GetAction(*lid_close_action);
      if (action) {
        ApplyValueAsMandatoryPolicy(*action, key::kLidCloseAction,
                                    &user_policy_map);
      }
      policy_dict.Remove(kLidCloseAction);
    }

    const base::Value* screen_dim_delay_scale =
        policy_dict.Find(kUserActivityScreenDimDelayScale);
    if (screen_dim_delay_scale) {
      ApplyValueAsMandatoryPolicy(*screen_dim_delay_scale,
                                  key::kUserActivityScreenDimDelayScale,
                                  &user_policy_map);
      policy_dict.Remove(kUserActivityScreenDimDelayScale);
    }

    // |policy_dict| is expected to be a valid value for the
    // PowerManagementIdleSettings policy now.
    if (!policy_dict.empty()) {
      ApplyValueAsMandatoryPolicy(base::Value(std::move(policy_dict)),
                                  key::kPowerManagementIdleSettings,
                                  &user_policy_map);
    }
  }

  UpdatePolicy(std::move(bundle));
}

}  // namespace policy
