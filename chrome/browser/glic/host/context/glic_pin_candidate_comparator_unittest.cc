// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pin_candidate_comparator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
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

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents1_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contents2_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  }

  void TearDown() override {
    web_contents1_.reset();
    web_contents2_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents1_;
  std::unique_ptr<content::WebContents> web_contents2_;
};

TEST_P(GlicPinCandidateComparatorParamTest, AllCases) {
  const auto& params = GetParam();
  SCOPED_TRACE(params.description);

  GlicPinCandidateComparator comparator(params.query);

  content::WebContentsTester::For(web_contents1_.get())
      ->SetTitle(params.title1);
  content::WebContentsTester::For(web_contents2_.get())
      ->SetTitle(params.title2);

  const auto now = base::TimeTicks::Now();
  content::WebContentsTester::For(web_contents1_.get())
      ->SetLastActiveTimeTicks(now + params.time1_delta);
  content::WebContentsTester::For(web_contents2_.get())
      ->SetLastActiveTimeTicks(now + params.time2_delta);

  EXPECT_EQ(params.expected,
            comparator(web_contents1_.get(), web_contents2_.get()));
  // Check that inverted ordering also holds.
  EXPECT_EQ(!params.expected,
            comparator(web_contents2_.get(), web_contents1_.get()));
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
