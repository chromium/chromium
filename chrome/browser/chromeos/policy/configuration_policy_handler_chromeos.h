// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "chromeos/network/network_ui_data.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace base {
class Value;
}

namespace policy {

class Schema;

// ConfigurationPolicyHandler for policies referencing external data.
class ExternalDataPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit ExternalDataPolicyHandler(const char* policy_name);
  ~ExternalDataPolicyHandler() override;

  // TypeCheckingPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalDataPolicyHandler);
};

// ConfigurationPolicyHandler for validation of the network configuration
// policies. These actually don't set any preferences, but the handler just
// generates error messages.
class NetworkConfigurationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  static NetworkConfigurationPolicyHandler* CreateForUserPolicy();
  static NetworkConfigurationPolicyHandler* CreateForDevicePolicy();

  ~NetworkConfigurationPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
  void PrepareForDisplaying(PolicyMap* policies) const override;

 private:
  explicit NetworkConfigurationPolicyHandler(
      const char* policy_name,
      ::onc::ONCSource onc_source,
      const char* pref_path);

  // Takes network policy in Value representation and produces an output Value
  // that contains a pretty-printed and sanitized version. In particular, we
  // remove any Passphrases that may be contained in the JSON. Ownership of the
  // return value is transferred to the caller.
  static std::unique_ptr<base::Value> SanitizeNetworkConfig(
      const base::Value* config);

  // The kind of ONC source that this handler represents. ONCSource
  // distinguishes between user and device policy.
  const ::onc::ONCSource onc_source_;

  // The name of the pref to apply the policy to.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationPolicyHandler);
};

// Maps the PinnedLauncherApps policy to the corresponding pref. List entries
// may be Android app ids or extension ids.
class PinnedLauncherAppsPolicyHandler : public ListPolicyHandler {
 public:
  PinnedLauncherAppsPolicyHandler();
  ~PinnedLauncherAppsPolicyHandler() override;

 protected:
  // ListPolicyHandler methods:

  // Returns true if |value| contains an Android app id (using a heuristic) or
  // an extension id.
  bool CheckListEntry(const base::Value& value) override;

  // Converts the list of strings |filtered_list| to a list of dictionaries and
  // sets the pref.
  void ApplyList(std::unique_ptr<base::ListValue> filtered_list,
                 PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PinnedLauncherAppsPolicyHandler);
};

class ScreenMagnifierPolicyHandler : public IntRangePolicyHandlerBase {
 public:
  ScreenMagnifierPolicyHandler();
  ~ScreenMagnifierPolicyHandler() override;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenMagnifierPolicyHandler);
};

// Policy handler for login screen power management settings. This does not
// actually set any prefs, it just checks whether the settings are valid and
// generates errors if appropriate.
class LoginScreenPowerManagementPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit LoginScreenPowerManagementPolicyHandler(const Schema& chrome_schema);
  ~LoginScreenPowerManagementPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenPowerManagementPolicyHandler);
};

// Handles the deprecated IdleAction policy, so both kIdleActionBattery and
// kIdleActionAC fall back to the deprecated action.
class DeprecatedIdleActionHandler : public IntRangePolicyHandlerBase {
 public:
  DeprecatedIdleActionHandler();
  ~DeprecatedIdleActionHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeprecatedIdleActionHandler);
};

class PowerManagementIdleSettingsPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit PowerManagementIdleSettingsPolicyHandler(
      const Schema& chrome_schema);
  ~PowerManagementIdleSettingsPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagementIdleSettingsPolicyHandler);
};

class ScreenLockDelayPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit ScreenLockDelayPolicyHandler(const Schema& chrome_schema);
  ~ScreenLockDelayPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenLockDelayPolicyHandler);
};

class ScreenBrightnessPercentPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit ScreenBrightnessPercentPolicyHandler(const Schema& chrome_schema);
  ~ScreenBrightnessPercentPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenBrightnessPercentPolicyHandler);
};

// Supported values for the |ArcBackupRestoreServiceEnabled| and
// |ArcGoogleLocationServicesEnabled| policies.
enum class ArcServicePolicyValue {
  kDisabled = 0,
  kUnderUserControl = 1,
  kEnabled = 2
};

// Instantiated once each for the |ArcBackupRestoreServiceEnabled| and
// |ArcGoogleLocationServicesEnabled| policies to handle their special logic:
// If the policy is set to |kUnderUserControl|, the pref is unmanaged, as if no
// policy was set.
class ArcServicePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  ArcServicePolicyHandler(const char* policy, const char* pref);

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const std::string pref_;

  DISALLOW_COPY_AND_ASSIGN(ArcServicePolicyHandler);
};

// Handles the |EcryptfsMigrationStrategy| policy, aliasing policy values that
// are no longer supported to the "migrate" option.
class EcryptfsMigrationStrategyPolicyHandler
    : public IntRangePolicyHandlerBase {
 public:
  EcryptfsMigrationStrategyPolicyHandler();

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EcryptfsMigrationStrategyPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
