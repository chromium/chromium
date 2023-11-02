// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_LANGUAGE_BLOCKLIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_LANGUAGE_BLOCKLIST_POLICY_HANDLER_H_

#include <vector>

#include "components/policy/core/browser/configuration_policy_handler.h"

// ConfigurationPolicyHandler for the SpellcheckLanguageBlocklist policy.
class SpellcheckLanguageBlocklistPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  explicit SpellcheckLanguageBlocklistPolicyHandler(const char* policy_name);

  SpellcheckLanguageBlocklistPolicyHandler(
      const SpellcheckLanguageBlocklistPolicyHandler&) = delete;
  SpellcheckLanguageBlocklistPolicyHandler& operator=(
      const SpellcheckLanguageBlocklistPolicyHandler&) = delete;

  ~SpellcheckLanguageBlocklistPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  void SortBlocklistedLanguages(const policy::PolicyMap& policies,
                                base::Value::List* const blocklisted,
                                std::vector<std::string>* const unknown,
                                std::vector<std::string>* const duplicates);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_LANGUAGE_BLOCKLIST_POLICY_HANDLER_H_
