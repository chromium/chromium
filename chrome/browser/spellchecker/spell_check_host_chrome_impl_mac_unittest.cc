// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

class SpellCheckHostChromeImplMacTest : public ::testing::Test {
 protected:
  void CombineResults(
      std::vector<SpellCheckResult>* remote_results,
      const std::vector<SpellCheckResult>& local_results) const {
    SpellCheckHostChromeImpl::CombineResultsForTesting(remote_results,
                                                       local_results);
  }
};

TEST_F(SpellCheckHostChromeImplMacTest, CombineResults) {
  std::vector<SpellCheckResult> local_results;
  std::vector<SpellCheckResult> remote_results;
  std::u16string remote_suggestion = u"remote";
  std::u16string local_suggestion = u"local";

  // Remote-only result - must be flagged as GRAMMAR after combine
  remote_results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 0, 5));

  // Local-only result - must be discarded after combine
  local_results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 10, 5));

  // local & remote result - must be flagged SPELLING, uses remote suggestion.
  SpellCheckResult result(SpellCheckResult::SPELLING, 20, 5, local_suggestion);
  local_results.push_back(result);
  result.replacements[0] = remote_suggestion;
  remote_results.push_back(result);

  CombineResults(&remote_results, local_results);

  ASSERT_EQ(2U, remote_results.size());
  EXPECT_EQ(SpellCheckResult::GRAMMAR, remote_results[0].decoration);
  EXPECT_EQ(0, remote_results[0].location);
  EXPECT_EQ(SpellCheckResult::SPELLING, remote_results[1].decoration);
  EXPECT_EQ(20, remote_results[1].location);
  EXPECT_EQ(remote_suggestion, remote_results[1].replacements[0]);
}
