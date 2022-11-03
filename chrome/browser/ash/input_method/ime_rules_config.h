// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace ash {
namespace input_method {

// IME config that implements a rule check for IME features.
class ImeRulesConfig {
 public:
  ImeRulesConfig(const ImeRulesConfig&) = delete;
  ImeRulesConfig& operator=(const ImeRulesConfig&) = delete;

  ~ImeRulesConfig();

  // Obtains the singleton instance.
  static ImeRulesConfig* GetInstance();

  // Runs the rule check against contextual info.
  bool IsAutoCorrectDisabled(const TextFieldContextualInfo& info);

  // Checks if domain is a sub-domain of info
 private:
  bool IsSubDomain(const TextFieldContextualInfo& info,
                   const std::string& domain);

 private:
  ImeRulesConfig();

  // For Singleton to be able to construct an instance.
  friend struct base::DefaultSingletonTraits<ImeRulesConfig>;

  friend class ImeRulesConfigTest;
  friend class ImeRulesConfigEnabledTest;
  friend class ImeRulesConfigDisabledTest;

  // Initializes the config from IME rules trial parameters. If there is
  // no trial or parsing fails, the rules will be empty and as such always
  // allow any features.
  void InitFromTrialParams();

  // The rule-based denylist of domains that will turn off auto_correct feature.
  std::vector<std::string> rule_auto_correct_domain_denylist_;
  // The default denylist of domains that will turn off auto_correct feature.
  std::vector<std::string> default_auto_correct_domain_denylist_{
      "amazon",
      "b.corp.google",
      "buganizer.corp.google",
      "cider.corp.google",
      "classroom.google",
      "desmos",
      "docs.google",
      "facebook",
      "instagram",
      "outlook.live",
      "outlook.office",
      "quizlet",
      "whatsapp",
      "youtube",
  };
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_
