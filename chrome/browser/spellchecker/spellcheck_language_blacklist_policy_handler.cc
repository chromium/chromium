// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_language_blacklist_policy_handler.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/strings/grit/components_strings.h"

SpellcheckLanguageBlacklistPolicyHandler::
    SpellcheckLanguageBlacklistPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kSpellcheckLanguageBlacklist,
                                base::Value::Type::LIST) {}

SpellcheckLanguageBlacklistPolicyHandler::
    ~SpellcheckLanguageBlacklistPolicyHandler() = default;

bool SpellcheckLanguageBlacklistPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  bool ok = CheckAndGetValue(policies, errors, &value);

  std::vector<base::Value> blacklisted;
  std::vector<std::string> unknown;
  std::vector<std::string> duplicates;
  SortBlacklistedLanguages(policies, &blacklisted, &unknown, &duplicates);

#if !defined(OS_MACOSX)
  for (const std::string language : duplicates) {
    errors->AddError(policy_name(), IDS_POLICY_SPELLCHECK_BLACKLIST_IGNORE,
                     language);
  }

  for (const std::string language : unknown) {
    errors->AddError(policy_name(), IDS_POLICY_SPELLCHECK_UNKNOWN_LANGUAGE,
                     language);
  }
#endif

  return ok;
}

void SpellcheckLanguageBlacklistPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // Ignore this policy if the SpellcheckEnabled policy disables spellcheck.
  const base::Value* spellcheck_enabled_value =
      policies.GetValue(policy::key::kSpellcheckEnabled);
  if (spellcheck_enabled_value && spellcheck_enabled_value->GetBool() == false)
    return;

  // If this policy isn't set, don't modify spellcheck languages.
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  // Set the blacklisted dictionaries preference based on this policy's values,
  // and emit warnings for unknown or duplicate languages.
  std::vector<base::Value> blacklisted;
  std::vector<std::string> unknown;
  std::vector<std::string> duplicates;
  SortBlacklistedLanguages(policies, &blacklisted, &unknown, &duplicates);

  for (const std::string language : duplicates) {
    SYSLOG(WARNING)
        << "SpellcheckLanguageBlacklist policy: an entry was also found in"
           " the SpellcheckLanguage policy: \""
        << language << "\". Blacklist entry will be ignored.";
  }

  for (const std::string language : unknown) {
    SYSLOG(WARNING) << "SpellcheckLanguageBlacklist policy: Unknown or "
                       "unsupported language \""
                    << language << "\"";
  }

  prefs->SetValue(spellcheck::prefs::kSpellCheckBlacklistedDictionaries,
                  base::Value(std::move(blacklisted)));
}

void SpellcheckLanguageBlacklistPolicyHandler::SortBlacklistedLanguages(
    const policy::PolicyMap& policies,
    std::vector<base::Value>* const blacklisted,
    std::vector<std::string>* const unknown,
    std::vector<std::string>* const duplicates) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  // Build a lookup of force-enabled spellcheck languages to find duplicates.
  const base::Value* forced_enabled_value =
      policies.GetValue(policy::key::kSpellcheckLanguage);
  std::unordered_set<std::string> forced_languages_lookup;
  if (forced_enabled_value) {
    for (const auto& forced_language : forced_enabled_value->GetList())
      forced_languages_lookup.insert(forced_language.GetString());
  }

  // Separate the valid languages from the unknown / unsupported languages and
  // the languages that also appear in the SpellcheckLanguage policy.
  base::span<const base::Value> blacklisted_languages = value->GetList();
  for (const base::Value& language : blacklisted_languages) {
    std::string current_language =
        spellcheck::GetCorrespondingSpellCheckLanguage(
            base::TrimWhitespaceASCII(language.GetString(), base::TRIM_ALL));

    if (current_language.empty()) {
      unknown->emplace_back(language.GetString());
    } else {
      if (forced_languages_lookup.find(language.GetString()) !=
          forced_languages_lookup.end()) {
        // If a language is both force-enabled and force-disabled, force-enable
        // wins. Put the language in the list of duplicates.
        duplicates->emplace_back(std::move(current_language));
      } else {
        blacklisted->emplace_back(std::move(current_language));
      }
    }
  }
}
