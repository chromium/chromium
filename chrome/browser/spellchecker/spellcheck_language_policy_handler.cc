// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_language_policy_handler.h"

#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/strings/grit/components_strings.h"

SpellcheckLanguagePolicyHandler::SpellcheckLanguagePolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kSpellcheckLanguage,
                                base::Value::Type::LIST) {}

SpellcheckLanguagePolicyHandler::~SpellcheckLanguagePolicyHandler() = default;

bool SpellcheckLanguagePolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  bool ok = CheckAndGetValue(policies, errors, &value);

  base::Value::List forced;
  std::vector<std::string> unknown;
  SortForcedLanguages(policies, &forced, &unknown);

#if !BUILDFLAG(IS_MAC)
  for (const auto& language : unknown) {
    errors->AddError(policy_name(), IDS_POLICY_SPELLCHECK_UNKNOWN_LANGUAGE,
                     language);
  }
#endif

  return ok;
}

void SpellcheckLanguagePolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // Ignore this policy if the SpellcheckEnabled policy disables spellcheck.
  const base::Value* spellcheck_enabled_value = policies.GetValue(
      policy::key::kSpellcheckEnabled, base::Value::Type::BOOLEAN);
  if (spellcheck_enabled_value && !spellcheck_enabled_value->GetBool())
    return;

  // If this policy isn't set, don't modify spellcheck languages.
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value)
    return;

  // Set the forced dictionaries preference based on this policy's values,
  // and emit warnings for unknown languages.
  base::Value::List forced;
  std::vector<std::string> unknown;
  SortForcedLanguages(policies, &forced, &unknown);

  for (const auto& language : unknown) {
    SYSLOG(WARNING)
        << "SpellcheckLanguage policy: Unknown or unsupported language \""
        << language << "\"";
  }

  prefs->SetValue(spellcheck::prefs::kSpellCheckEnable, base::Value(true));
  prefs->SetValue(spellcheck::prefs::kSpellCheckForcedDictionaries,
                  base::Value(std::move(forced)));
}

void SpellcheckLanguagePolicyHandler::SortForcedLanguages(
    const policy::PolicyMap& policies,
    base::Value::List* const forced,
    std::vector<std::string>* const unknown) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value)
    return;

  // Separate the valid languages from the unknown / unsupported languages.
  for (const base::Value& language : value->GetList()) {
    std::string candidate_language(
        base::TrimWhitespaceASCII(language.GetString(), base::TRIM_ALL));
    std::string current_language =
        SpellcheckService::GetSupportedAcceptLanguageCode(candidate_language);

    if (current_language.empty()) {
      unknown->emplace_back(language.GetString());
    } else {
      forced->Append(std::move(current_language));
    }
  }
}
