// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/launcher_search/search_util.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

TEST(SearchUtilTest, ProviderTypes) {
  const int types = ProviderTypes();
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_OPEN_TAB);
}

// Tests result conversion for a default answer result.
TEST(SearchUtilTest, CreateAnswerResult) {
  AutocompleteMatch match;
  match.relevance = 1248;
  match.destination_url = GURL("http://www.example.com/");
  match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST;
  match.contents = u"contents";
  match.description = u"description";

  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional one\", \"tt\": 42 } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"text two\", \"tt\": 5 }], "
      "              \"at\": { \"t\": \"additional two\", \"tt\": 6 } } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(SuggestionAnswer::ParseAnswer(value->GetDict(), u"-1", &answer));
  match.answer = answer;

  const auto result = CreateAnswerResult(match, nullptr, AutocompleteInput());
  EXPECT_EQ(result->type, mojom::SearchResultType::kOmniboxResult);
  EXPECT_EQ(result->relevance, 1248);
  ASSERT_TRUE(result->destination_url.has_value());
  EXPECT_EQ(result->destination_url.value(), GURL("http://www.example.com/"));
  EXPECT_EQ(result->is_omnibox_search,
            mojom::SearchResult::OptionalBool::kTrue);
  EXPECT_EQ(result->is_answer, mojom::SearchResult::OptionalBool::kTrue);

  ASSERT_TRUE(result->contents.has_value());
  EXPECT_EQ(result->contents.value(), u"contents");
  ASSERT_TRUE(result->additional_contents.has_value());
  EXPECT_EQ(result->additional_contents.value(), u"additional one");
  ASSERT_TRUE(result->description.has_value());
  EXPECT_EQ(result->description.value(), u"text two");
  ASSERT_TRUE(result->additional_description.has_value());
  EXPECT_EQ(result->additional_description.value(), u"additional two");
}

// Tests result conversion for a rich entity Omnibox result.
TEST(SearchUtilTest, CreateResult) {
  AutocompleteMatch match;
  match.relevance = 300;
  match.destination_url = GURL("http://www.example.com/");
  match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY;
  match.image_url = GURL("http://www.example.com/image.jpeg");

  match.contents = u"contents";
  match.description = u"description";
  match.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::URL)};
  match.description_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  const auto result = CreateResult(match, nullptr, nullptr, nullptr, u"query",
                                   AutocompleteInput());
  EXPECT_EQ(result->type, mojom::SearchResultType::kOmniboxResult);
  EXPECT_EQ(result->relevance, 300);
  ASSERT_TRUE(result->destination_url.has_value());
  EXPECT_EQ(result->destination_url.value(), GURL("http://www.example.com/"));
  EXPECT_EQ(result->is_omnibox_search,
            mojom::SearchResult::OptionalBool::kTrue);
  EXPECT_EQ(result->is_answer, mojom::SearchResult::OptionalBool::kFalse);
  EXPECT_EQ(result->omnibox_type, mojom::SearchResult::OmniboxType::kRichImage);
  ASSERT_TRUE(result->image_url.has_value());
  EXPECT_EQ(result->image_url.value(),
            GURL("http://www.example.com/image.jpeg"));

  ASSERT_TRUE(result->contents.has_value());
  EXPECT_EQ(result->contents.value(), u"contents");
  ASSERT_TRUE(result->description.has_value());
  EXPECT_EQ(result->description.value(), u"description");

  // The URL text class should be retained, but MATCH should be ignored.
  EXPECT_EQ(result->contents_type, mojom::SearchResult::TextType::kUrl);
  EXPECT_EQ(result->description_type, mojom::SearchResult::TextType::kUnset);
}

}  // namespace
}  // namespace crosapi
