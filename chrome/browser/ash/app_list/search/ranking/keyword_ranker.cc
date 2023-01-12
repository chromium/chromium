// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/keyword_ranker.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/scoring.h"

namespace app_list {

KeywordRanker::KeywordRanker() = default;

KeywordRanker::~KeywordRanker() = default;

void KeywordRanker::Start(const std::u16string& query,
                          ResultsMap& results,
                          CategoriesList& categories) {
  // TODO(b/263059094): when the user start input, this function will
  // be called.
  last_query_ = query;

  // TODO(b/263816068): Use real keyword extraction function when ready.

  // Stores the providers that match with the keyword within the input query.
  matched_providers_ = ExtractKeyword(last_query_).second;
}

void KeywordRanker::UpdateResultRanks(ResultsMap& results,
                                      ProviderType provider) {
  // TODO(b/263059094): update the result by boost the scores that
  // match certain keywords, the rest remain unchanged.

  // Return if the given provider matched a keyword in the query
  // as this does not require modification of results.
  if (std::find(matched_providers_.begin(), matched_providers_.end(),
                provider) != matched_providers_.end()) {
    return;
  }

  const auto it = results.find(provider);
  if (it == results.end()) {
    return;
  }

  for (auto& result : it->second) {
    result->scoring().set_keyword_multiplier(0.9);
  }
}

}  // namespace app_list
