// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_types.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"

namespace app_list {
namespace {

// Tests result conversion for a default answer result.
// Tests result conversion for a rich entity Omnibox result.
TEST(OmniboxUtilTest, CreateResult) {
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

  const auto result =
      CreateResult(match, nullptr, nullptr, AutocompleteInput());
  EXPECT_EQ(result->relevance, 300);
  EXPECT_EQ(result->destination_url, GURL("http://www.example.com/"));
  EXPECT_TRUE(result->is_omnibox_search);
  EXPECT_FALSE(result->is_answer);
  EXPECT_EQ(result->omnibox_type, OmniboxResultType::kSearch);
  ASSERT_TRUE(result->image_url.has_value());
  EXPECT_EQ(result->image_url.value(),
            GURL("http://www.example.com/image.jpeg"));

  ASSERT_TRUE(result->contents.has_value());
  EXPECT_EQ(result->contents.value(), u"contents");
  ASSERT_TRUE(result->description.has_value());
  EXPECT_EQ(result->description.value(), u"description");

  // The URL text class should be retained, but MATCH should be ignored.
  EXPECT_EQ(result->contents_type, OmniboxTextType::kUrl);
  EXPECT_EQ(result->description_type, OmniboxTextType::kUnset);
}

// Tests result conversion for a weather Omnibox result.
// Tests result conversion for a calculator result. A calculator result can
// either have a description or no description; both possibilities are tested.
TEST(OmniboxUtilTest, CreateCalculatorResult) {
  // A match with the input in contents and the answer in desc.
  AutocompleteMatch match_desc;
  match_desc.relevance = 300;
  match_desc.type = AutocompleteMatchType::CALCULATOR;
  match_desc.destination_url = GURL("https://www.example.com.au/calc?q=1+2");
  match_desc.contents = u"1+2";
  match_desc.description = u"3";
  match_desc.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};
  match_desc.description_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  // A match with the answer in content and no desc.
  AutocompleteMatch match_no_desc;
  match_no_desc.relevance = 300;
  match_no_desc.type = AutocompleteMatchType::CALCULATOR;
  match_no_desc.destination_url = GURL("https://www.example.com.au/calc?q=1+2");
  match_no_desc.contents = u"3";
  match_no_desc.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  for (const auto& match : {match_desc, match_no_desc}) {
    const auto result = CreateAnswerResult(match, /*controller=*/nullptr,
                                           u"1+2", AutocompleteInput());
    EXPECT_EQ(result->relevance, 300);
    EXPECT_EQ(result->destination_url,
              GURL("https://www.example.com.au/calc?q=1+2"));
    EXPECT_TRUE(result->is_omnibox_search);
    EXPECT_TRUE(result->is_answer);
    EXPECT_EQ(result->answer_type, OmniboxResultAnswerType::kCalculator);
    ASSERT_TRUE(result->contents.has_value());
    EXPECT_EQ(result->contents.value(), u"1+2");
    ASSERT_TRUE(result->description.has_value());
    EXPECT_EQ(result->description.value(), u"3");
    EXPECT_EQ(result->contents_type, OmniboxTextType::kUnset);
    EXPECT_EQ(result->description_type, OmniboxTextType::kUnset);
  }
}

}  // namespace
}  // namespace app_list
