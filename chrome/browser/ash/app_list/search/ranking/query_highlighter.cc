// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/query_highlighter.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"

namespace app_list {
namespace {

using TextVector = std::vector<ash::SearchResultTextItem>;
using CrosApiSearchResult = crosapi::mojom::SearchResult;

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= ash::SearchResultTag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= ash::SearchResultTag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= ash::SearchResultTag::DIM;

  return tag_styles;
}

void AppendMatchTags(const std::u16string& query,
                     const std::u16string& text,
                     ChromeSearchResult::Tags* tags) {
  const auto matches = FindTermMatches(query, text);
  const auto classes =
      ClassifyTermMatches(matches, text.length(),
                          /*match_style=*/ACMatchClassification::MATCH,
                          /*non_match_style=*/ACMatchClassification::NONE);

  int tag_styles = ash::SearchResultTag::NONE;
  size_t tag_start = 0;

  for (const ACMatchClassification& text_class : classes) {
    // Closes current tag.
    if (tag_styles != ash::SearchResultTag::NONE) {
      tags->push_back(
          ash::SearchResultTag(tag_styles, tag_start, text_class.offset));
      tag_styles = ash::SearchResultTag::NONE;
    }

    int style = text_class.style;
    if (style == ACMatchClassification::NONE) {
      continue;
    }

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(style);
  }

  if (tag_styles != ash::SearchResultTag::NONE) {
    tags->push_back(ash::SearchResultTag(tag_styles, tag_start, text.length()));
  }
}

void SetMatchTags(const std::u16string& query, TextVector& text_vector) {
  for (auto& item : text_vector) {
    if (item.GetType() != ash::SearchResultTextItemType::kString)
      continue;

    ash::SearchResultTags tags = item.GetTextTags();
    // Remove any existing match tags before re-calculating them.
    for (auto& tag : tags) {
      tag.styles &= ~ash::SearchResultTag::MATCH;
    }
    AppendMatchTags(query, item.GetText(), &tags);
    item.SetTextTags(tags);
  }
}

}  // namespace

QueryHighlighter::QueryHighlighter() = default;
QueryHighlighter::~QueryHighlighter() = default;

void QueryHighlighter::Start(const std::u16string& query,
                             const CategoriesList& categories) {
  last_query_ = query;
}

void QueryHighlighter::UpdateResultRanks(ResultsMap& results,
                                         ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    TextVector title_vector = result->title_text_vector();
    SetMatchTags(last_query_, title_vector);
    result->SetTitleTextVector(title_vector);

    if (result->display_type() !=
        ChromeSearchResult::DisplayType::kAnswerCard) {
      TextVector details_vector = result->details_text_vector();
      SetMatchTags(last_query_, details_vector);
      result->SetDetailsTextVector(details_vector);
    }
  }
}

}  // namespace app_list
