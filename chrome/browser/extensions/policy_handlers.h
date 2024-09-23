// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_
#define CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_

#include <optional>

#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class PolicyMap;
class PolicyErrorMap;
class Schema;
}  // namespace policy

namespace extensions {

// Implements additional checks for policies that are lists of extension IDs.
class ExtensionListPolicyHandler : public policy::ListPolicyHandler {
 public:
  ExtensionListPolicyHandler(const char* policy_name,
                             const char* pref_path,
                             bool allow_wildcards);

  ExtensionListPolicyHandler(const ExtensionListPolicyHandler&) = delete;
  ExtensionListPolicyHandler& operator=(const ExtensionListPolicyHandler&) =
      delete;

  ~ExtensionListPolicyHandler() override;

 protected:
  // ListPolicyHandler methods:

  // Checks whether |value| contains a valid extension id (or a wildcard).
  bool CheckListEntry(const base::Value& value) override;

  // Sets |prefs| at pref_path() to |filtered_list|.
  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;

 private:
  const char* pref_path_;
  bool allow_wildcards_;
};

// Class for parsing the list of extensions to force install.
//
// On ChromeOS the policy values will be filtered before updating the prefs,
// such that the prefs on Ash only contain the extensions that must be force
// installed on Ash.
// Similarly the prefs on Lacros will only contain the extensions that must
// be force installed on Lacros.
class ExtensionInstallForceListPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionInstallForceListPolicyHandler();
  ExtensionInstallForceListPolicyHandler(
      const ExtensionInstallForceListPolicyHandler&) = delete;
  ExtensionInstallForceListPolicyHandler& operator=(
      const ExtensionInstallForceListPolicyHandler&) = delete;
  ~ExtensionInstallForceListPolicyHandler() override = default;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns a `base::Value::Dict` with the extensions that must be force
  // installed in Ash. If Lacros is disabled this is the full extensions list,
  // and if Lacros is enabled this only contains the extensions that must run on
  // the Ash side.
  //
  // Returns nullopt if the policy is unset.
  std::optional<base::Value::Dict> GetAshPolicyDict(
      const policy::PolicyMap& policy_map);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  // Returns a `base::Value::Dict` with the extensions that must be force
  // installed in Lacros.
  //
  // Returns nullopt if the policy is unset.
  std::optional<base::Value::Dict> GetLacrosPolicyDict(
      const policy::PolicyMap& policy_map);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Returns a `base::Value::Dict` with the extensions that must be force
  // installed.
  //
  // Returns nullopt if the policy is unset.
  std::optional<base::Value::Dict> GetPolicyDict(
      const policy::PolicyMap& policy_map);

 private:
  // Parses the data in |policy_value| and writes them to |extension_dict|.
  bool ParseList(const base::Value* policy_value,
                 base::Value::Dict* extension_dict,
                 policy::PolicyErrorMap* errors);
};

// Class for parsing the list of extensions that are blocklisted.
class ExtensionInstallBlockListPolicyHandler
    : public policy::ConfigurationPolicyHandler {
 public:
  ExtensionInstallBlockListPolicyHandler();
  ExtensionInstallBlockListPolicyHandler(
      const ExtensionInstallBlockListPolicyHandler&) = delete;
  ExtensionInstallBlockListPolicyHandler& operator=(
      const ExtensionInstallBlockListPolicyHandler&) = delete;
  ~ExtensionInstallBlockListPolicyHandler() override;

  // `ConfigurationPolicyHandler`:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  ExtensionListPolicyHandler list_handler_;
};

// Implements additional checks for policies that are lists of extension
// URLPatterns.
class ExtensionURLPatternListPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionURLPatternListPolicyHandler(const char* policy_name,
                                       const char* pref_path);

  ExtensionURLPatternListPolicyHandler(
      const ExtensionURLPatternListPolicyHandler&) = delete;
  ExtensionURLPatternListPolicyHandler& operator=(
      const ExtensionURLPatternListPolicyHandler&) = delete;

  ~ExtensionURLPatternListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* pref_path_;
};

class ExtensionSettingsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ExtensionSettingsPolicyHandler(const policy::Schema& chrome_schema);

  ExtensionSettingsPolicyHandler(const ExtensionSettingsPolicyHandler&) =
      delete;
  ExtensionSettingsPolicyHandler& operator=(
      const ExtensionSettingsPolicyHandler&) = delete;

  ~ExtensionSettingsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Performs sanitization for both Check/ApplyPolicySettings(). If an entry
  // in |dict_value| doesn't pass validation, that entry is removed from the
  // dictionary. Validation errors are stored in |errors| if non-null.
  void SanitizePolicySettings(base::Value* dict_value,
                              policy::PolicyErrorMap* errors);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_
