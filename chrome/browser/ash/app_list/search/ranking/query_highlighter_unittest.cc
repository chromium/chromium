// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/query_highlighter.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using Tag = ash::SearchResultTag;

Results MakeResults(const std::vector<std::u16string>& titles) {
  Results results;
  for (const auto& title : titles) {
    auto result = std::make_unique<TestResult>();
    result->SetTitle(title);
    result->SetTitleTags({Tag(Tag::URL | Tag::MATCH, 0, title.length())});
    results.push_back(std::move(result));
  }
  return results;
}

}  // namespace

TEST(QueryHighlighterTest, KeepExistingNonMatchTags) {
  QueryHighlighter highlighter;

  ResultsMap results;
  CategoriesList categories({{.category = Category::kWeb}});
  highlighter.Start(u"example", categories);

  results[ResultType::kOmnibox] = MakeResults({u"example_result"});

  // The example result is initialized with a URL tag.
  auto tags =
      results[ResultType::kOmnibox][0]->title_text_vector()[0].GetTextTags();
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_TRUE(tags[0].styles & Tag::URL);

  highlighter.UpdateResultRanks(results, ProviderType::kOmnibox);

  // The URL tag should still be there.
  tags = results[ResultType::kOmnibox][0]->title_text_vector()[0].GetTextTags();
  ASSERT_GE(tags.size(), 1u);
  EXPECT_TRUE(tags[0].styles & Tag::URL);
}

TEST(QueryHighlighterTest, RemoveExistingMatchTags) {
  QueryHighlighter highlighter;

  ResultsMap results;
  CategoriesList categories({{.category = Category::kWeb}});
  highlighter.Start(u"example", categories);

  results[ResultType::kOmnibox] = MakeResults({u"example_result"});

  // The example result is initialized with a MATCH tag.
  auto tags =
      results[ResultType::kOmnibox][0]->title_text_vector()[0].GetTextTags();
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_TRUE(tags[0].styles & Tag::MATCH);

  highlighter.UpdateResultRanks(results, ProviderType::kOmnibox);

  // The MATCH tag should be removed.
  tags = results[ResultType::kOmnibox][0]->title_text_vector()[0].GetTextTags();
  ASSERT_GE(tags.size(), 1u);
  EXPECT_FALSE(tags[0].styles & Tag::MATCH);
}

TEST(QueryHighlighterTest, AppendMatchTags) {
  QueryHighlighter highlighter;

  ResultsMap results;
  CategoriesList categories({{.category = Category::kWeb}});
  highlighter.Start(u"example", categories);

  results[ResultType::kOmnibox] = MakeResults({u"example_result"});

  // The example result is initialized with one tag.
  auto tags =
      results[ResultType::kOmnibox][0]->title_text_vector()[0].GetTextTags();
  EXPECT_EQ(tags.size(), 1u);

  highlighter.UpdateResultRanks(results, ProviderType::kOmnibox);

  // The query highlighter should have appended more tags, all of which are
  // MATCH only.
  tags = results[ResultType::kOmnibox][0]->title_text_vector()[0].GetTextTags();
  ASSERT_GT(tags.size(), 1u);
  for (size_t i = 1; i < tags.size(); ++i) {
    EXPECT_EQ(tags[i].styles, Tag::MATCH);
  }
}

}  // namespace app_list::test
