// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_answer_result.h"

#include <optional>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_types.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"

namespace app_list::test {
namespace {

using Tag = ash::SearchResultTag;

class OmniboxAnswerResultTest : public testing::Test {
 public:
  OmniboxAnswerResultTest() = default;
  ~OmniboxAnswerResultTest() override = default;
};

MATCHER_P(TagEquals, tag, "") {
  bool styles_match = arg.styles == tag.styles;
  bool range_match = arg.range == tag.range;
  return styles_match && range_match;
}

}  // namespace

TEST_F(OmniboxAnswerResultTest, CalculatorResult) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::CALCULATOR;
  match.contents = u"2+2";
  match.description = u"4";

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      CreateAnswerResult(match, /*controller=*/nullptr, u"query",
                         AutocompleteInput()),
      u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_CALCULATOR);

  ASSERT_EQ(result.title_text_vector().size(), 1u);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"2+2");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1u);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"4");
  EXPECT_TRUE(details.GetTextTags().empty());
  EXPECT_EQ(result.answer_type(), OmniboxResultAnswerType::kCalculator);

  std::stringstream out;
  out << result;
  EXPECT_EQ(out.str(),
            "omnibox_answer:// {0.00 | nr:0.00 rs:0.00 bm:-1 cr:-1 bi:0}");
}

TEST_F(OmniboxAnswerResultTest, CalculatorResultNoDescription) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::CALCULATOR;
  match.contents = u"4";

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      CreateAnswerResult(match, /*controller=*/nullptr, u"2+2",
                         AutocompleteInput()),
      u"2+2");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_CALCULATOR);

  ASSERT_EQ(result.title_text_vector().size(), 1u);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"2+2");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1u);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"4");
  EXPECT_TRUE(details.GetTextTags().empty());
  EXPECT_EQ(result.answer_type(), OmniboxResultAnswerType::kCalculator);
}

}  // namespace app_list::test
