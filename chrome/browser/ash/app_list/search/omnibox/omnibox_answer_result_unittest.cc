// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_answer_result.h"

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr, u"query",
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
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kCalculator);

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
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr, u"2+2",
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
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kCalculator);
}

TEST_F(OmniboxAnswerResultTest, WeatherResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_WEATHER.
  const std::u16string kWeatherType = u"8";

  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional one\", \"tt\": 42 } } }, "
      "  { \"il\": { \"al\": \"accessibility label\", "
      "              \"t\": [{ \"t\": \"-5°C\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional two\", \"tt\": 42 } } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(
      SuggestionAnswer::ParseAnswer(value->GetDict(), kWeatherType, &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";
  match.description = u"description";

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr, u"query",
                                  AutocompleteInput()),
      u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_ANSWER);

  ASSERT_EQ(result.big_title_text_vector().size(), 1u);
  const auto& big_title = result.big_title_text_vector()[0];
  ASSERT_EQ(big_title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(big_title.GetText(), u"-5");
  EXPECT_TRUE(big_title.GetTextTags().empty());

  ASSERT_EQ(result.big_title_superscript_text_vector().size(), 1u);
  const auto& superscript = result.big_title_superscript_text_vector()[0];
  ASSERT_EQ(superscript.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(superscript.GetText(), u"°C");
  EXPECT_TRUE(big_title.GetTextTags().empty());

  ASSERT_EQ(result.title_text_vector().size(), 1u);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"accessibility label");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1u);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"additional two");
  EXPECT_TRUE(details.GetTextTags().empty());

  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kWeather);
}

TEST_F(OmniboxAnswerResultTest, AnswerResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_FINANCE.
  const std::u16string kWeatherType = u"2";

  SuggestionAnswer answer;
  // Text tags ("tt") 5 and 6 are SuggestionAnswer::TextType::NEGATIVE and
  // SuggestionAnswer::TextType::POSITIVE respectively.
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional one\", \"tt\": 42 } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"text two\", \"tt\": 5 }], "
      "              \"at\": { \"t\": \"additional two\", \"tt\": 6 } } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(
      SuggestionAnswer::ParseAnswer(value->GetDict(), kWeatherType, &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";
  match.description = u"description";

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr, u"query",
                                  AutocompleteInput()),
      u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_ANSWER);

  const auto& title = result.title_text_vector();
  ASSERT_EQ(title.size(), 3u);
  ASSERT_EQ(title[0].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title[0].GetText(), u"contents");
  EXPECT_TRUE(title[0].GetTextTags().empty());

  ASSERT_EQ(title[1].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title[1].GetText(), u" ");
  EXPECT_TRUE(title[1].GetTextTags().empty());

  ASSERT_EQ(title[2].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title[2].GetText(), u"additional one");
  EXPECT_TRUE(title[2].GetTextTags().empty());

  // The NEGATIVE text type should have RED tags, and the POSITIVE text type
  // should have GREEN tags.
  const auto& details = result.details_text_vector();
  ASSERT_EQ(details.size(), 3u);
  ASSERT_EQ(details[0].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details[0].GetText(), u"text two");
  size_t length = details[0].GetText().length();
  EXPECT_THAT(details[0].GetTextTags(), testing::UnorderedElementsAre(TagEquals(
                                            Tag(Tag::Style::RED, 0, length))));

  ASSERT_EQ(details[1].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details[1].GetText(), u" ");
  length = details[1].GetText().length();
  EXPECT_TRUE(details[1].GetTextTags().empty());

  ASSERT_EQ(details[2].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details[2].GetText(), u"additional two");
  length = details[2].GetText().length();
  EXPECT_THAT(details[2].GetTextTags(),
              testing::UnorderedElementsAre(
                  TagEquals(Tag(Tag::Style::GREEN, 0, length))));

  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kFinance);
}

TEST_F(OmniboxAnswerResultTest, DictionaryResultMultiline) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_DICTIONARY.
  const std::u16string kDictionaryType = u"1";

  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"text two\", \"tt\": 5 }] } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(SuggestionAnswer::ParseAnswer(value->GetDict(), kDictionaryType,
                                            &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";
  match.description = u"description";

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr, u"query",
                                  AutocompleteInput()),
      u"query");
  EXPECT_TRUE(result.multiline_details());
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kDictionary);
}

TEST_F(OmniboxAnswerResultTest, TranslationResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_TRANSLATION.
  const int kTranslationType = 7;
  SuggestionAnswer answer;
  answer.set_type(kTranslationType);

  AutocompleteMatch match;
  match.answer = answer;

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr,
                                  u"hello in Spanish", AutocompleteInput()),
      u"hello in Spanish");
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kTranslation);
}

TEST_F(OmniboxAnswerResultTest, CurrencyResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_CURRENCY.
  const int kCurrencyType = 10;
  SuggestionAnswer answer;
  answer.set_type(kCurrencyType);

  AutocompleteMatch match;
  match.answer = answer;

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr,
                                  u"100 usd in aud", AutocompleteInput()),
      u"100 usd in aud");
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kCurrency);
}

TEST_F(OmniboxAnswerResultTest, SunriseResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_SUNRISE.
  const int kSunriseType = 6;
  SuggestionAnswer answer;
  answer.set_type(kSunriseType);

  AutocompleteMatch match;
  match.answer = answer;

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr,
                                  u"sunrise time in Sydney",
                                  AutocompleteInput()),
      u"sunrise time in Sydney");
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kSunrise);
}

TEST_F(OmniboxAnswerResultTest, WhenIsResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_WHEN_IS.
  const int kWhenIsType = 9;
  SuggestionAnswer answer;
  answer.set_type(kWhenIsType);

  AutocompleteMatch match;
  match.answer = answer;

  OmniboxAnswerResult result(
      /*profile=*/nullptr, /*list_controller=*/nullptr,
      crosapi::CreateAnswerResult(match, /*controller=*/nullptr,
                                  u"when is christmas", AutocompleteInput()),
      u"when is christmas");
  EXPECT_EQ(result.answer_type(),
            crosapi::mojom::SearchResult::AnswerType::kWhenIs);
}

}  // namespace app_list::test
