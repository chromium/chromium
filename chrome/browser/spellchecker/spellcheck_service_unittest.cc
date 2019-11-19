// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <ostream>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestCase {
  TestCase(const std::string& accept_languages,
           const std::string& unsplit_spellcheck_dictionaries,
           const std::string& unsplit_expected_languages,
           const std::string& unsplit_expected_languages_used_for_spellcheck)
      : accept_languages(accept_languages),
        spellcheck_dictionaries(
            base::SplitString(unsplit_spellcheck_dictionaries,
                              ",",
                              base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    std::vector<std::string> languages =
        base::SplitString(unsplit_expected_languages, ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::vector<std::string> used_for_spellcheck =
        base::SplitString(unsplit_expected_languages_used_for_spellcheck, ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    SpellcheckService::Dictionary dictionary;
    for (const auto& language : languages) {
      dictionary.language = language;
      dictionary.used_for_spellcheck =
          base::Contains(used_for_spellcheck, language);
      expected_dictionaries.push_back(dictionary);
    }
  }

  ~TestCase() {}

  std::string accept_languages;
  std::vector<std::string> spellcheck_dictionaries;
  std::vector<SpellcheckService::Dictionary> expected_dictionaries;
};

bool operator==(const SpellcheckService::Dictionary& lhs,
                const SpellcheckService::Dictionary& rhs) {
  return lhs.language == rhs.language &&
         lhs.used_for_spellcheck == rhs.used_for_spellcheck;
}

std::ostream& operator<<(std::ostream& out,
                         const SpellcheckService::Dictionary& dictionary) {
  out << "{\"" << dictionary.language << "\", used_for_spellcheck="
      << (dictionary.used_for_spellcheck ? "true " : "false") << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "language::prefs::kAcceptLanguages=[" << test_case.accept_languages
      << "], prefs::kSpellCheckDictionaries=["
      << base::JoinString(test_case.spellcheck_dictionaries, ",")
      << "], expected=[";
  for (const auto& dictionary : test_case.expected_dictionaries) {
    out << dictionary << ",";
  }
  out << "]";
  return out;
}

class SpellcheckServiceUnitTest : public testing::TestWithParam<TestCase> {
 public:
  SpellcheckServiceUnitTest() {
    user_prefs::UserPrefs::Set(&context_, &prefs_);
  }
  ~SpellcheckServiceUnitTest() override {}

  void SetUp() override {
    prefs()->registry()->RegisterListPref(
        spellcheck::prefs::kSpellCheckDictionaries);
    prefs()->registry()->RegisterStringPref(language::prefs::kAcceptLanguages,
                                            std::string());
  }

  base::SupportsUserData* context() { return &context_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  struct : public base::SupportsUserData {
  } context_;
  TestingPrefServiceSimple prefs_;
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceUnitTest);
};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceUnitTest,
    testing::Values(
        TestCase("en,aa", "aa", "", ""),
        TestCase("en,en-JP,fr,aa", "fr", "fr", "fr"),
        TestCase("en,en-JP,fr,zz,en-US", "fr", "fr,en-US", "fr"),
        TestCase("en,en-US,en-GB", "en-GB", "en-US,en-GB", "en-GB"),
        TestCase("en,en-US,en-AU", "en-AU", "en-US,en-AU", "en-AU"),
        TestCase("en,en-US,en-AU", "en-US", "en-US,en-AU", "en-US"),
        TestCase("en,en-US", "en-US", "en-US", "en-US"),
        TestCase("en,en-US,fr", "en-US", "en-US,fr", "en-US"),
        TestCase("en,fr,en-US,en-AU", "en-US,fr", "fr,en-US,en-AU", "fr,en-US"),
        TestCase("en-US,en", "en-US", "en-US", "en-US"),
        TestCase("hu-HU,hr-HR", "hr", "hu,hr", "hr")));

TEST_P(SpellcheckServiceUnitTest, GetDictionaries) {
  prefs()->SetString(language::prefs::kAcceptLanguages,
                     GetParam().accept_languages);
  base::ListValue spellcheck_dictionaries;
  spellcheck_dictionaries.AppendStrings(GetParam().spellcheck_dictionaries);
  prefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
               spellcheck_dictionaries);

  std::vector<SpellcheckService::Dictionary> dictionaries;
  SpellcheckService::GetDictionaries(context(), &dictionaries);

  EXPECT_EQ(GetParam().expected_dictionaries, dictionaries);
}
