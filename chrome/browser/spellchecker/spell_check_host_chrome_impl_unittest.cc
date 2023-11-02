// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(USE_RENDERER_SPELLCHECKER)
#error BUILDFLAG(USE_RENDERER_SPELLCHECKER) is required for these tests.
#endif

class TestSpellCheckHostChromeImpl {
 public:
  TestSpellCheckHostChromeImpl()
      : spellcheck_(std::make_unique<SpellcheckService>(&testing_profile_)) {}

  TestSpellCheckHostChromeImpl(const TestSpellCheckHostChromeImpl&) = delete;
  TestSpellCheckHostChromeImpl& operator=(const TestSpellCheckHostChromeImpl&) =
      delete;

  SpellcheckCustomDictionary& GetCustomDictionary() const {
    EXPECT_NE(nullptr, spellcheck_.get());
    SpellcheckCustomDictionary* custom_dictionary =
        spellcheck_->GetCustomDictionary();
    return *custom_dictionary;
  }

  std::vector<SpellCheckResult> FilterCustomWordResults(
      const std::string& text,
      const std::vector<SpellCheckResult>& service_results) const {
    return SpellCheckHostChromeImpl::FilterCustomWordResults(
        text, GetCustomDictionary(), service_results);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<SpellcheckService> spellcheck_;
};

// Spelling corrections of custom dictionary words should be removed from the
// results returned by the remote Spelling service.
TEST(SpellCheckHostChromeImplTest, CustomSpellingResults) {
  std::vector<SpellCheckResult> service_results;
  service_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 0, 6, u"Hello"));
  service_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 7, 5, u"World"));
  TestSpellCheckHostChromeImpl host_impl;
  host_impl.GetCustomDictionary().AddWord("Helllo");
  std::vector<SpellCheckResult> results =
      host_impl.FilterCustomWordResults("Helllo Warld", service_results);
  ASSERT_EQ(1u, results.size());

  EXPECT_EQ(service_results[1].decoration, results[0].decoration);
  EXPECT_EQ(service_results[1].location, results[0].location);
  EXPECT_EQ(service_results[1].length, results[0].length);
  EXPECT_EQ(service_results[1].replacements.size(),
            results[0].replacements.size());
  EXPECT_EQ(service_results[1].replacements[0], results[0].replacements[0]);
}

// Spelling corrections of words that are not in the custom dictionary should
// be retained in the results returned by the remote Spelling service.
TEST(SpellCheckHostChromeImplTest, SpellingServiceResults) {
  std::vector<SpellCheckResult> service_results;
  service_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 0, 6, u"Hello"));
  service_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 7, 5, u"World"));
  TestSpellCheckHostChromeImpl host_impl;
  host_impl.GetCustomDictionary().AddWord("Hulo");
  std::vector<SpellCheckResult> results =
      host_impl.FilterCustomWordResults("Helllo Warld", service_results);
  ASSERT_EQ(service_results.size(), results.size());

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(service_results[i].decoration, results[i].decoration);
    EXPECT_EQ(service_results[i].location, results[i].location);
    EXPECT_EQ(service_results[i].length, results[i].length);
    EXPECT_EQ(service_results[i].replacements.size(),
              results[i].replacements.size());
    EXPECT_EQ(service_results[i].replacements[0], results[i].replacements[0]);
  }
}
