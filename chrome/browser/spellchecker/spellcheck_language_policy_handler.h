// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_LANGUAGE_POLICY_HANDLER_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_LANGUAGE_POLICY_HANDLER_H_

#include <vector>

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

// ConfigurationPolicyHandler for the SpellcheckLanguage policy.
class SpellcheckLanguagePolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  SpellcheckLanguagePolicyHandler();
  ~SpellcheckLanguagePolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  void SortForcedLanguages(const policy::PolicyMap& policies,
                           std::vector<base::Value>* const forced,
                           std::vector<std::string>* const unknown);
  DISALLOW_COPY_AND_ASSIGN(SpellcheckLanguagePolicyHandler);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_LANGUAGE_POLICY_HANDLER_H_
