// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_CONFIGURATION_POLICY_HANDLER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_CONFIGURATION_POLICY_HANDLER_ASH_H_

#include <string>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

class Schema;

// ConfigurationPolicyHandler for policies referencing external data.
class ExternalDataPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit ExternalDataPolicyHandler(const char* policy_name);

  ExternalDataPolicyHandler(const ExternalDataPolicyHandler&) = delete;
  ExternalDataPolicyHandler& operator=(const ExternalDataPolicyHandler&) =
      delete;

  ~ExternalDataPolicyHandler() override;

  static bool CheckPolicySettings(const char* policy,
                                  const PolicyMap::Entry* entry,
                                  PolicyErrorMap* errors);

  // TypeCheckingPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

// ConfigurationPolicyHandler for validation of the network configuration
// policies. These actually don't set any preferences, but the handler just
// generates error messages.
class NetworkConfigurationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  static NetworkConfigurationPolicyHandler* CreateForUserPolicy();
  static NetworkConfigurationPolicyHandler* CreateForDevicePolicy();

  NetworkConfigurationPolicyHandler(const NetworkConfigurationPolicyHandler&) =
      delete;
  NetworkConfigurationPolicyHandler& operator=(
      const NetworkConfigurationPolicyHandler&) = delete;

  ~NetworkConfigurationPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
  void PrepareForDisplaying(PolicyMap* policies) const override;

 private:
  explicit NetworkConfigurationPolicyHandler(const char* policy_name,
                                             ::onc::ONCSource onc_source,
                                             const char* pref_path);

  // Takes network policy in Value representation and produces an output Value
  // that contains a pretty-printed and sanitized version. In particular, we
  // remove any Passphrases that may be contained in the JSON. Ownership of the
  // return value is transferred to the caller.
  static std::optional<base::Value> SanitizeNetworkConfig(
      const base::Value* config);

  // The kind of ONC source that this handler represents. ONCSource
  // distinguishes between user and device policy.
  const ::onc::ONCSource onc_source_;

  // The name of the pref to apply the policy to.
  const char* pref_path_;
};

// Maps the PinnedLauncherApps policy to the corresponding pref. List entries
// may be Android app ids or extension ids.
class PinnedLauncherAppsPolicyHandler : public ListPolicyHandler {
 public:
  PinnedLauncherAppsPolicyHandler();

  PinnedLauncherAppsPolicyHandler(const PinnedLauncherAppsPolicyHandler&) =
      delete;
  PinnedLauncherAppsPolicyHandler& operator=(
      const PinnedLauncherAppsPolicyHandler&) = delete;

  ~PinnedLauncherAppsPolicyHandler() override;

 protected:
  // ListPolicyHandler methods:

  // Returns true if |value| contains an Android app id (using a heuristic) or
  // an extension id.
  bool CheckListEntry(const base::Value& value) override;

  // Converts the list of strings |filtered_list| to a list of dictionaries and
  // sets the pref.
  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;
};

// Maps the DefaultHandlersForFileExtensions policy to the corresponding pref.
class DefaultHandlersForFileExtensionsPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit DefaultHandlersForFileExtensionsPolicyHandler(const policy::Schema&);

  // SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

  bool IsValidPolicyId(std::string_view policy_id) const;
};

class ScreenMagnifierPolicyHandler : public IntRangePolicyHandlerBase {
 public:
  ScreenMagnifierPolicyHandler();

  ScreenMagnifierPolicyHandler(const ScreenMagnifierPolicyHandler&) = delete;
  ScreenMagnifierPolicyHandler& operator=(const ScreenMagnifierPolicyHandler&) =
      delete;

  ~ScreenMagnifierPolicyHandler() override;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

// Policy handler for login screen power management settings. This does not
// actually set any prefs, it just checks whether the settings are valid and
// generates errors if appropriate.
class LoginScreenPowerManagementPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit LoginScreenPowerManagementPolicyHandler(const Schema& chrome_schema);

  LoginScreenPowerManagementPolicyHandler(
      const LoginScreenPowerManagementPolicyHandler&) = delete;
  LoginScreenPowerManagementPolicyHandler& operator=(
      const LoginScreenPowerManagementPolicyHandler&) = delete;

  ~LoginScreenPowerManagementPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

// Handles the deprecated IdleAction policy, so both kIdleActionBattery and
// kIdleActionAC fall back to the deprecated action.
class DeprecatedIdleActionHandler : public IntRangePolicyHandlerBase {
 public:
  DeprecatedIdleActionHandler();

  DeprecatedIdleActionHandler(const DeprecatedIdleActionHandler&) = delete;
  DeprecatedIdleActionHandler& operator=(const DeprecatedIdleActionHandler&) =
      delete;

  ~DeprecatedIdleActionHandler() override;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

class PowerManagementIdleSettingsPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit PowerManagementIdleSettingsPolicyHandler(
      const Schema& chrome_schema);

  PowerManagementIdleSettingsPolicyHandler(
      const PowerManagementIdleSettingsPolicyHandler&) = delete;
  PowerManagementIdleSettingsPolicyHandler& operator=(
      const PowerManagementIdleSettingsPolicyHandler&) = delete;

  ~PowerManagementIdleSettingsPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

class ScreenLockDelayPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit ScreenLockDelayPolicyHandler(const Schema& chrome_schema);

  ScreenLockDelayPolicyHandler(const ScreenLockDelayPolicyHandler&) = delete;
  ScreenLockDelayPolicyHandler& operator=(const ScreenLockDelayPolicyHandler&) =
      delete;

  ~ScreenLockDelayPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

class ScreenBrightnessPercentPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit ScreenBrightnessPercentPolicyHandler(const Schema& chrome_schema);

  ScreenBrightnessPercentPolicyHandler(
      const ScreenBrightnessPercentPolicyHandler&) = delete;
  ScreenBrightnessPercentPolicyHandler& operator=(
      const ScreenBrightnessPercentPolicyHandler&) = delete;

  ~ScreenBrightnessPercentPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
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

  ArcServicePolicyHandler(const ArcServicePolicyHandler&) = delete;
  ArcServicePolicyHandler& operator=(const ArcServicePolicyHandler&) = delete;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const std::string pref_;
};

// Instantiated for the `ArcGoogleLocationServicesEnabled` policy. This
// overrides the old handling of the `ArcGoogleLocationServicesEnabled` policy
// when the Privacy Hub location is rolled out.
class ArcLocationServicePolicyHandler : public ArcServicePolicyHandler {
 public:
  explicit ArcLocationServicePolicyHandler(const char* policy,
                                           const char* pref);

  ArcLocationServicePolicyHandler(const ArcLocationServicePolicyHandler&) =
      delete;
  ArcLocationServicePolicyHandler& operator=(
      const ArcLocationServicePolicyHandler&) = delete;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

// Maps Chrome Compose policy into ChromeOS Orca settings
class HelpMeWritePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  enum class HelpMeWritePolicyValue {
    kEnabledWithModelImprovement = 0,
    kEnabledWithoutModelImprovement = 1,
    kDisabled = 2,
  };

  HelpMeWritePolicyHandler();

  HelpMeWritePolicyHandler(const HelpMeWritePolicyHandler&) = delete;
  HelpMeWritePolicyHandler& operator=(const HelpMeWritePolicyHandler&) = delete;

  ~HelpMeWritePolicyHandler() override = default;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_CONFIGURATION_POLICY_HANDLER_ASH_H_
