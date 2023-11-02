// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {
namespace i18n {
namespace {

constexpr const wchar_t* kExactMatchCandidates[] = {
    L"am",  L"ar",    L"bg",    L"bn",    L"ca",     L"cs", L"da", L"de",
    L"el",  L"en-gb", L"en-us", L"es",    L"es-419", L"et", L"fa", L"fi",
    L"fil", L"fr",    L"gu",    L"hi",    L"hr",     L"hu", L"id", L"it",
    L"iw",  L"ja",    L"kn",    L"ko",    L"lt",     L"lv", L"ml", L"mr",
    L"nl",  L"no",    L"pl",    L"pt-br", L"pt-pt",  L"ro", L"ru", L"sk",
    L"sl",  L"sr",    L"sv",    L"sw",    L"ta",     L"te", L"th", L"tr",
    L"uk",  L"vi",    L"zh-cn", L"zh-tw"};

constexpr const wchar_t* kAliasMatchCandidates[] = {
    L"he",      L"nb",      L"tl",    L"zh-chs", L"zh-cht",
    L"zh-hans", L"zh-hant", L"zh-hk", L"zh-mo"};

constexpr const wchar_t* kWildcardMatchCandidates[] = {L"en-AU", L"es-CO",
                                                       L"pt-AB", L"zh-SG"};

std::vector<LanguageSelector::LangToOffset> MakeLanguageOffsetPairs() {
  std::vector<LanguageSelector::LangToOffset> language_offset_pairs;
  int i = 0;
  for (const wchar_t* lang : kExactMatchCandidates) {
    language_offset_pairs.emplace_back(lang, i++);
  }

  return language_offset_pairs;
}

class TestLanguageSelector : public LanguageSelector {
 public:
  TestLanguageSelector() : TestLanguageSelector(std::vector<std::wstring>()) {}
  explicit TestLanguageSelector(const std::vector<std::wstring>& candidates)
      : TestLanguageSelector(candidates, MakeLanguageOffsetPairs()) {}
  TestLanguageSelector(
      const std::vector<std::wstring>& candidates,
      span<const LanguageSelector::LangToOffset> languages_to_offset)
      : LanguageSelector(candidates, languages_to_offset) {}
};

}  // namespace

// Test that a language is selected from the system.
TEST(LanguageSelectorTest, DefaultSelection) {
  TestLanguageSelector instance;
  EXPECT_FALSE(instance.matched_candidate().empty());
}

// Test some hypothetical candidate sets.
TEST(LanguageSelectorTest, AssortedSelections) {
  {
    std::vector<std::wstring> candidates = {L"fr-BE", L"fr", L"en"};
    TestLanguageSelector instance(candidates);
    // Expect the exact match to win.
    EXPECT_EQ(L"fr", instance.matched_candidate());
  }
  {
    std::vector<std::wstring> candidates = {L"xx-YY", L"cc-Ssss-RR"};
    TestLanguageSelector instance(candidates);
    // Expect the fallback to win.
    EXPECT_EQ(L"en-us", instance.matched_candidate());
  }
  {
    std::vector<std::wstring> candidates = {L"zh-SG", L"en-GB"};
    TestLanguageSelector instance(candidates);
    // Expect the alias match to win.
    EXPECT_EQ(L"zh-SG", instance.matched_candidate());
  }
}

// A fixture for testing sets of single-candidate selections.
class LanguageSelectorMatchCandidateTest
    : public ::testing::TestWithParam<const wchar_t*> {};

TEST_P(LanguageSelectorMatchCandidateTest, TestMatchCandidate) {
  TestLanguageSelector instance({GetParam()});
  EXPECT_EQ(GetParam(), instance.matched_candidate());
}

// Test that all existing translations can be found by exact match.
INSTANTIATE_TEST_SUITE_P(TestExactMatches,
                         LanguageSelectorMatchCandidateTest,
                         ::testing::ValuesIn(kExactMatchCandidates));

// Test the alias matches.
INSTANTIATE_TEST_SUITE_P(TestAliasMatches,
                         LanguageSelectorMatchCandidateTest,
                         ::testing::ValuesIn(kAliasMatchCandidates));

// Test a few wildcard matches.
INSTANTIATE_TEST_SUITE_P(TestWildcardMatches,
                         LanguageSelectorMatchCandidateTest,
                         ::testing::ValuesIn(kWildcardMatchCandidates));

// A fixture for testing aliases that match to an expected translation.  The
// first member of the tuple is the expected translation, the second is a
// candidate that should be aliased to the expectation.
class LanguageSelectorAliasTest
    : public ::testing::TestWithParam<
          std::tuple<const wchar_t*, const wchar_t*>> {};

// Test that the candidate language maps to the aliased translation.
TEST_P(LanguageSelectorAliasTest, AliasesMatch) {
  TestLanguageSelector instance({std::get<1>(GetParam())});
  EXPECT_EQ(std::get<0>(GetParam()), instance.selected_translation());
}

INSTANTIATE_TEST_SUITE_P(EnGbAliases,
                         LanguageSelectorAliasTest,
                         ::testing::Combine(::testing::Values(L"en-gb"),
                                            ::testing::Values(L"en-au",
                                                              L"en-ca",
                                                              L"en-nz",
                                                              L"en-za")));

INSTANTIATE_TEST_SUITE_P(IwAliases,
                         LanguageSelectorAliasTest,
                         ::testing::Combine(::testing::Values(L"iw"),
                                            ::testing::Values(L"he")));

INSTANTIATE_TEST_SUITE_P(NoAliases,
                         LanguageSelectorAliasTest,
                         ::testing::Combine(::testing::Values(L"no"),
                                            ::testing::Values(L"nb")));

INSTANTIATE_TEST_SUITE_P(FilAliases,
                         LanguageSelectorAliasTest,
                         ::testing::Combine(::testing::Values(L"fil"),
                                            ::testing::Values(L"tl")));

INSTANTIATE_TEST_SUITE_P(
    ZhCnAliases,
    LanguageSelectorAliasTest,
    ::testing::Combine(::testing::Values(L"zh-cn"),
                       ::testing::Values(L"zh-chs", L"zh-hans", L"zh-sg")));

INSTANTIATE_TEST_SUITE_P(ZhTwAliases,
                         LanguageSelectorAliasTest,
                         ::testing::Combine(::testing::Values(L"zh-tw"),
                                            ::testing::Values(L"zh-cht",
                                                              L"zh-hant",
                                                              L"zh-hk",
                                                              L"zh-mo")));

// Test that we can get a match of the default language.
TEST(LanguageSelectorTest, DefaultLanguageName) {
  TestLanguageSelector instance;
  EXPECT_FALSE(instance.selected_translation().empty());
}

// All languages given to the selector must be lower cased (since generally
// the language names are generated by a python script).
TEST(LanguageSelectorTest, InvalidLanguageCasing) {
  constexpr LanguageSelector::LangToOffset kLangToOffset[] = {{L"en-US", 0}};
  EXPECT_DCHECK_DEATH(LanguageSelector instance(
      std::vector<std::wstring>({L"en-us"}), kLangToOffset));
}

// Language name and offset pairs must be ordered when generated by the
// python script.
TEST(LanguageSelectorTest, InvalidLanguageNameOrder) {
  constexpr LanguageSelector::LangToOffset kLangToOffset[] = {{L"en-us", 0},
                                                              {L"en-gb", 1}};
  EXPECT_DCHECK_DEATH(LanguageSelector instance(
      std::vector<std::wstring>({L"en-us"}), kLangToOffset));
}

// There needs to be a fallback language available in the generated
// languages if ever the selector is given a language that does not exist.
TEST(LanguageSelectorTest, NoFallbackLanguageAvailable) {
  constexpr LanguageSelector::LangToOffset kLangToOffset[] = {{L"en-gb", 0}};
  EXPECT_DCHECK_DEATH(LanguageSelector instance(
      std::vector<std::wstring>({L"aa-bb"}), kLangToOffset));
}

// No languages available.
TEST(LanguageSelectorTest, NoLanguagesAvailable) {
  EXPECT_DCHECK_DEATH(
      LanguageSelector instance(std::vector<std::wstring>({L"en-us"}), {}));
}

}  // namespace i18n
}  // namespace win
}  // namespace base
