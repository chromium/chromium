// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium style is to have one unit test per one header file. However, the
// applied blocked spellcheck language policy depends on the applied forced
// language policy. If a language is both blocked and forced, forced wins. It is
// only practical to test this interaction in a single unit test covering both
// header files.
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_language_blocklist_policy_handler.h"
#include "chrome/browser/spellchecker/spellcheck_language_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

struct TestCase {
  TestCase(const std::vector<std::string>& blocked_languages,
           const std::vector<std::string>& forced_languages,
           const std::vector<std::string>& expected_blocked_languages,
           const std::vector<std::string>& expected_forced_languages,
           bool spellcheck_enabled,
           bool windows_spellchecker_enabled)
      : blocked_languages(blocked_languages),
        forced_languages(forced_languages),
        expected_blocked_languages(expected_blocked_languages),
        expected_forced_languages(expected_forced_languages),
        spellcheck_enabled(spellcheck_enabled),
        windows_spellchecker_enabled(windows_spellchecker_enabled) {}

  std::vector<std::string> blocked_languages;
  std::vector<std::string> forced_languages;
  std::vector<std::string> expected_blocked_languages;
  std::vector<std::string> expected_forced_languages;
  bool spellcheck_enabled;
  bool windows_spellchecker_enabled;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "blocked_languages=["
      << base::JoinString(test_case.blocked_languages, ",")
      << "], forced_languages=["
      << base::JoinString(test_case.forced_languages, ",")
      << "], expected_blocked_languages=["
      << base::JoinString(test_case.expected_blocked_languages, ",")
      << "], expected_forced_languages=["
      << base::JoinString(test_case.expected_forced_languages, ",")
      << "], spellcheck_enabled=" << test_case.spellcheck_enabled
      << "], windows_spellchecker_enabled="
      << test_case.windows_spellchecker_enabled;
  return out;
}

class SpellcheckLanguagePolicyHandlersTest
    : public testing::TestWithParam<TestCase> {
 public:
  SpellcheckLanguagePolicyHandlersTest() = default;
  ~SpellcheckLanguagePolicyHandlersTest() override = default;

  void CheckPrefs(const PrefValueMap& prefs,
                  const std::string& key,
                  const std::vector<std::string>& expected) {
    // Retrieve the spellcheck enabled pref (if it exists).
    const base::Value* spellcheck_enabled_pref = nullptr;
    const bool is_spellcheck_enabled_pref_set = prefs.GetValue(
        spellcheck::prefs::kSpellCheckEnable, &spellcheck_enabled_pref);

    const base::Value* languages_list = nullptr;
    if (GetParam().spellcheck_enabled) {
      EXPECT_TRUE(is_spellcheck_enabled_pref_set);
      EXPECT_TRUE(spellcheck_enabled_pref->is_bool());
      EXPECT_TRUE(spellcheck_enabled_pref->GetBool());

      EXPECT_TRUE(prefs.GetValue(key, &languages_list));
      EXPECT_TRUE(languages_list->is_list());
      EXPECT_EQ(expected.size(), languages_list->GetList().size());

      for (const auto& language : languages_list->GetList()) {
        EXPECT_TRUE(language.is_string());
        EXPECT_TRUE(base::Contains(expected, language.GetString()));
      }
    } else {
      EXPECT_FALSE(is_spellcheck_enabled_pref_set);
      // No language list should be added to prefs if spellchecking disabled.
      EXPECT_FALSE(prefs.GetValue(key, &languages_list));
    }
  }
};

TEST_P(SpellcheckLanguagePolicyHandlersTest, ApplyPolicySettings) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  std::optional<spellcheck::ScopedDisableBrowserSpellCheckerForTesting>
      disable_browser_spell_checker;
  if (!GetParam().windows_spellchecker_enabled) {
    // Hunspell-only spellcheck languages will be used.
    disable_browser_spell_checker.emplace();
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  PrefValueMap prefs;
  policy::PolicyMap policy;

  base::Value::List blocked_languages_list;
  for (const auto& blocked_language : GetParam().blocked_languages) {
    blocked_languages_list.Append(std::move(blocked_language));
  }

  base::Value::List forced_languages_list;
  for (const auto& forced_language : GetParam().forced_languages) {
    forced_languages_list.Append(std::move(forced_language));
  }

  policy.Set(policy::key::kSpellcheckLanguageBlocklist,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             base::Value(std::move(blocked_languages_list)), nullptr);

  policy.Set(policy::key::kSpellcheckLanguage, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             base::Value(std::move(forced_languages_list)), nullptr);

  policy.Set(policy::key::kSpellcheckEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             base::Value(GetParam().spellcheck_enabled), nullptr);

  // Apply policy to the forced languages handler.
  SpellcheckLanguagePolicyHandler forced_languages_handler;
  forced_languages_handler.ApplyPolicySettings(policy, &prefs);

  // Apply policy to the blocked languages handler.
  SpellcheckLanguageBlocklistPolicyHandler blocked_languages_handler(
      policy::key::kSpellcheckLanguageBlocklist);
  blocked_languages_handler.ApplyPolicySettings(policy, &prefs);

  // Check if forced languages preferences are as expected.
  CheckPrefs(prefs, spellcheck::prefs::kSpellCheckForcedDictionaries,
             GetParam().expected_forced_languages);

  // Check if blocked languages preferences are as expected.
  CheckPrefs(prefs, spellcheck::prefs::kSpellCheckBlocklistedDictionaries,
             GetParam().expected_blocked_languages);
}

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckLanguagePolicyHandlersTest,
    testing::Values(
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
        // Test cases for Windows spellchecker (policy languages not restricted
        // to Hunspell).
        TestCase({"ar-SA", "es-MX", "fi", "fr",
                  "not-a-language"} /* blocked languages */,
                 {"fi", "fr", "not-another-language"} /* forced languages */,
                 {"ar", "es-MX"} /* expected blocked languages */,
                 {"fi", "fr"} /* expected forced languages */,
                 true /* spellcheck enabled */,
                 true /* windows spellchecker enabled */),
        TestCase({"ar-SA", "es-MX", "fi", "fr "} /* blocked languages */,
                 {"fi", "fr"} /* forced languages */,
                 {""} /* expected blocked languages */,
                 {""} /* expected forced languages */,
                 false /* spellcheck enabled */,
                 true /* windows spellchecker enabled */),
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
        // Test cases for Hunspell only spellchecker. ar-SA and fi are
        // non-Hunspell languages so are ignored for policy enforcement. If they
        // ever obtain Hunspell support, the first test case below will fail.
        TestCase({"ar-SA", "es-MX", "fi", "fr",
                  "not-a-language"} /* blocked languages */,
                 {"fi", "fr", "not-another-language"} /* forced languages */,
                 {"es-MX"} /* expected blocked languages */,
                 {"fr"} /* expected forced languages */,
                 true /* spellcheck enabled */,
                 false /* windows spellchecker enabled */),
        TestCase({"ar-SA", "es-MX", "fi", "fr",
                  "not-a-language"} /* blocked languages */,
                 {"fi", "fr", "not-another-language"} /* forced languages */,
                 {""} /* expected blocked languages */,
                 {""} /* expected forced languages */,
                 false /* spellcheck enabled */,
                 false /* windows spellchecker enabled */)));

}  // namespace policy
