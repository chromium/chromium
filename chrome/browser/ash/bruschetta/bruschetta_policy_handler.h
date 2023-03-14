// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace bruschetta {

// BruschettaPolicyHandler is responsible for mapping the
// BruschettaVMConfiguration enterprise policy into chrome preferences. Because
// we do non-trivial mapping we have our own subclass. Right now we:
// - Fill in optional policies values with their default (e.g. vtpm)
// - Map the appropriate architecture specific downloads to the corresponding
//   generic keys ("installer_image_x86_64" to "installer_image" and so on)
// - Replace string-enumerations with numeric values defined in
//   bruschetta_pref_names.h
// - Reduce the enable level if necessary to match what we can actually offer
//   i.e. we don't mark a config as installable if we can't actually install it.
//
// For concrete examples, see
// //components/policy/test/data/policy_test_cases.json
class BruschettaPolicyHandler
    : public policy::SimpleSchemaValidatingPolicyHandler {
 public:
  explicit BruschettaPolicyHandler(policy::Schema schema);
  ~BruschettaPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool CheckDownloadableObject(policy::PolicyErrorMap* errors,
                               const std::string& id,
                               const std::string& key,
                               const base::Value::Dict& dict);

  // The set of configurations that have been downgraded from installable to
  // runnable due to an error in their config. Filled by CheckPolicySettings and
  // used by ApplyPolicySettings.
  base::flat_set<std::string> downgraded_by_error_;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_POLICY_HANDLER_H_
