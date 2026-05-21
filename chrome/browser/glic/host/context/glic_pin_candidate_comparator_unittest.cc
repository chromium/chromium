// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pin_candidate_comparator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

struct GlicPinComparatorTestParams {
  // Test case description used for `SCOPED_TRACE`.
  std::string description;
  std::optional<std::string> query;
  std::u16string title1;
  std::u16string title2;
  base::TimeDelta time1_delta;
  base::TimeDelta time2_delta;
  bool expected = false;
};

class GlicPinCandidateComparatorParamTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<GlicPinComparatorTestParams> {
 public:
  GlicPinCandidateComparatorParamTest() {
    // The comparator depends on ICU, which must be initialized.
    base::test::InitializeICUForTesting();
  }

 protected:
  tabs::MockTabInterface tab1_;
  tabs::MockTabInterface tab2_;
};

TEST_P(GlicPinCandidateComparatorParamTest, AllCases) {
  const auto& params = GetParam();
  SCOPED_TRACE(params.description);

  GlicPinCandidateComparator comparator(params.query);

  EXPECT_CALL(tab1_, GetTitle()).WillRepeatedly(testing::Return(params.title1));
  EXPECT_CALL(tab2_, GetTitle()).WillRepeatedly(testing::Return(params.title2));

  const auto now = base::Time::Now();
  EXPECT_CALL(tab1_, GetLastActiveTime())
      .WillRepeatedly(testing::Return(now + params.time1_delta));
  EXPECT_CALL(tab2_, GetLastActiveTime())
      .WillRepeatedly(testing::Return(now + params.time2_delta));

  EXPECT_EQ(params.expected, comparator(&tab1_, &tab2_));
  // Check that inverted ordering also holds.
  EXPECT_EQ(!params.expected, comparator(&tab2_, &tab1_));
}

INSTANTIATE_TEST_SUITE_P(
    GlicPinCandidateComparatorTests,
    GlicPinCandidateComparatorParamTest,
    testing::Values(
        GlicPinComparatorTestParams{.description = "NoQueryFirstIsMoreRecent",
                                    .query = std::nullopt,
                                    .title1 = u"alpha",
                                    .title2 = u"alpha",
                                    .time2_delta = base::Seconds(-1),
                                    .expected = true},
        GlicPinComparatorTestParams{.description = "NoQuerySecondIsMoreRecent",
                                    .query = std::nullopt,
                                    .title1 = u"alpha",
                                    .title2 = u"alpha",
                                    .time1_delta = base::Seconds(-1),
                                    .expected = false},
        GlicPinComparatorTestParams{.description = "PrefixMatchFirstIsBetter",
                                    .query = "a",
                                    .title1 = u"alpha",
                                    .title2 = u"beta",
                                    .expected = true},
        GlicPinComparatorTestParams{.description = "PrefixMatchSecondIsBetter",
                                    .query = "a",
                                    .title1 = u"beta",
                                    .title2 = u"alpha",
                                    .expected = false},
        GlicPinComparatorTestParams{.description = "WordMatchFirstIsBetter",
                                    .query = "b",
                                    .title1 = u"alpha beta",
                                    .title2 = u"gamma",
                                    .expected = true},
        GlicPinComparatorTestParams{.description = "WordMatchSecondIsBetter",
                                    .query = "b",
                                    .title1 = u"gamma",
                                    .title2 = u"alpha beta",
                                    .expected = false},
        GlicPinComparatorTestParams{.description = "ContainsMatchFirstIsBetter",
                                    .query = "l",
                                    .title1 = u"alpha",
                                    .title2 = u"beta",
                                    .expected = true},
        GlicPinComparatorTestParams{
            .description = "ContainsMatchSecondIsBetter",
            .query = "l",
            .title1 = u"beta",
            .title2 = u"alpha",
            .expected = false},
        GlicPinComparatorTestParams{
            .description = "FallbackToMruFirstIsMoreRecent",
            .query = "z",
            .title1 = u"alpha",
            .title2 = u"alpha",
            .time2_delta = base::Seconds(-1),
            .expected = true},
        GlicPinComparatorTestParams{
            .description = "FallbackToMruSecondIsMoreRecent",
            .query = "z",
            .title1 = u"alpha",
            .title2 = u"alpha",
            .time1_delta = base::Seconds(-1),
            .expected = false}),
    [](const testing::TestParamInfo<
        GlicPinCandidateComparatorParamTest::ParamType>& info) {
      return info.param.description;
    });

}  // namespace

}  // namespace glic
