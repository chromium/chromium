// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spelling_request.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

struct RemoteCheckTestCase {
  std::string test_name;
  std::u16string text;
  std::vector<SpellCheckResult> initial_results;
  std::vector<SpellCheckResult> expected_converted_results;
};

SpellCheckResult MakeResult(int pos, int length) {
  return SpellCheckResult(SpellCheckResult::Decoration::SPELLING, pos, length,
                          u"");
}

class SpellingRequestRemoteCheckUnitTest
    : public testing::TestWithParam<RemoteCheckTestCase> {
 public:
  SpellingRequestRemoteCheckUnitTest() {
    spelling_request_ = SpellingRequest::CreateForTest(
        u"", base::DoNothing(), base::DoNothing(), base::DoNothing());
  }

  ~SpellingRequestRemoteCheckUnitTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SpellingRequest> spelling_request_;
};

// Helper for setting the test case's name.
static std::string DescribeParams(
    const testing::TestParamInfo<RemoteCheckTestCase>& info) {
  return info.param.test_name;
}

std::vector<RemoteCheckTestCase> BuildTestCases() {
  std::vector<RemoteCheckTestCase> test_cases = {
      RemoteCheckTestCase{"no_results_all_ascii",
                          u"This has no spelling mistakes 1 2 3",
                          {},
                          {}},
      RemoteCheckTestCase{
          "some_results_all_ascii",
          u"Tihs has 3 speling mistkes",
          {MakeResult(0, 4), MakeResult(11, 7), MakeResult(19, 7)},
          {MakeResult(0, 4), MakeResult(11, 7), MakeResult(19, 7)}},
      RemoteCheckTestCase{
          "no_results_non_ascii",
          u"\u00e9\u00c8\u00e7\u00e2\u062c\u305c\u000a\u0020\u00ef",
          {},
          {}},
      RemoteCheckTestCase{"some_results_non_ascii",
                          u"\u00e9\u00c8\u00e7 tihs\u00e2\u062c is "
                          u"\u305c\u000a a \u0020 mistke\u00ef",
                          {MakeResult(4, 4), MakeResult(21, 6)},
                          {MakeResult(4, 4), MakeResult(21, 6)}},
      RemoteCheckTestCase{
          "no_results_some_surrogate_pairs",
          u"👨‍👩‍👦‍👦 This 😁 has 🧑🏿 emojis",
          // The code point representation of the emojis in the above string is:
          // "\ud83d\udc68\u200d\ud83d\udc69\u200d\ud83d\udc66\u200d\ud83d\udc66
          //     This \ud83d\ude01 has \ud83e\uddd1\ud83c\udfff emojis"
          {},
          {}},
      RemoteCheckTestCase{
          "some_results_some_surrogate_pairs",
          u"👨‍👩‍👦‍👦 Tihs 😁 has 🧑🏿 emjis",
          // The code point representation of the emojis in the above string is:
          // "\ud83d\udc68\u200d\ud83d\udc69\u200d\ud83d\udc66\u200d\ud83d\udc66
          //     Tihs \ud83d\ude01 has \ud83e\uddd1\ud83c\udfff emjis"
          {MakeResult(8, 4), MakeResult(22, 5)},
          {MakeResult(12, 4), MakeResult(29, 5)}},
      RemoteCheckTestCase{
          "surrogate_pairs_inside_word",
          u"I ufort👨‍👩‍👦‍👦😁🧑🏿unately cant",
          // The code point representation of the emojis in the above string is:
          // "I ufort\ud83d\udc68\u200d\ud83d\udc69\u200d\ud83d\udc66\u200d
          //     \ud83d\udc66\ud83d\ude01\ud83e\uddd1\ud83c\udfffunately cant"
          {MakeResult(2, 22), MakeResult(25, 4)},
          {MakeResult(2, 29), MakeResult(32, 4)}}};

  return test_cases;
}

INSTANTIATE_TEST_SUITE_P(/* No prefix */,
                         SpellingRequestRemoteCheckUnitTest,
                         testing::ValuesIn(BuildTestCases()),
                         DescribeParams);

// Tests that remote results have their position value correctly adjusted to
// the corresponding code point position.
TEST_P(SpellingRequestRemoteCheckUnitTest, OnRemoteCheckCompleted) {
  const RemoteCheckTestCase& param = GetParam();
  spelling_request_->OnRemoteCheckCompleted(true, param.text,
                                            param.initial_results);
  const std::vector<SpellCheckResult>& actual_converted_results =
      spelling_request_->remote_results_;

  EXPECT_EQ(actual_converted_results.size(),
            param.expected_converted_results.size());
  for (const auto& expected : param.expected_converted_results) {
    bool found = false;

    for (const auto& actual : actual_converted_results) {
      if (expected.decoration == actual.decoration &&
          expected.location == actual.location &&
          expected.length == actual.length) {
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found) << "Result with pos = " << expected.location
                       << " and length = " << expected.length << " not found.";
  }
}
