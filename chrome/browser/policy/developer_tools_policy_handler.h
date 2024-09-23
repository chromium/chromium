// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_HANDLER_H_

#include "chrome/browser/policy/extension_developer_mode_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

// Handles the DeveloperToolsDisabled and DeveloperToolsAvailability policies.
// Controls the managed values of the prefs |kDevToolsAvailability| and
// |kExtensionsUIDeveloperMode|.
class DeveloperToolsPolicyHandler : public ConfigurationPolicyHandler {
 public:
  DeveloperToolsPolicyHandler();
  DeveloperToolsPolicyHandler(const DeveloperToolsPolicyHandler&) = delete;
  DeveloperToolsPolicyHandler& operator=(const DeveloperToolsPolicyHandler&) =
      delete;
  ~DeveloperToolsPolicyHandler() override;

  // Developer tools availability as set by policy. The values must match the
  // 'DeveloperToolsAvailability' policy definition.
  enum class Availability {
    // Default: Developer tools are allowed, except for policy-installed
    // extensions and, if this is a managed profile, component extensions.
    kDisallowedForForceInstalledExtensions = 0,
    // Developer tools allowed in all contexts.
    kAllowed = 1,
    // Developer tools disallowed in all contexts.
    kDisallowed = 2,
    // Maximal valid value for range checking.
    kMaxValue = kDisallowed
  };

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

  // Registers the pref for policy-set developer tools availability in
  // |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the effective developer tools availability for the profile.
  static Availability GetEffectiveAvailability(Profile* profile);

 private:
  // This instance should only be used for calling IsValidPolicySet() and not
  // for applying the policy settings. The latter is done by the instance which
  // is added in `ConfigurationPolicyHandlerList`.
  ExtensionDeveloperModePolicyHandler extension_developer_mode_policy_handler_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_HANDLER_H_
