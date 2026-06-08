// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/developer_tools_availability.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/policy/extension_developer_mode_policy_handler.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

class Profile;

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

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

  // Returns the effective developer tools availability for the profile.
  static DeveloperToolsAvailability GetEffectiveAvailability(Profile* profile);

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // This instance should only be used for calling IsValidPolicySet() and not
  // for applying the policy settings. The latter is done by the instance which
  // is added in `ConfigurationPolicyHandlerList`.
  ExtensionDeveloperModePolicyHandler extension_developer_mode_policy_handler_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_HANDLER_H_
