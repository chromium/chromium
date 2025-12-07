// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_QUERY_HIGHLIGHTER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_QUERY_HIGHLIGHTER_H_

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// A ranker that postprocesses result tags so that any text that matches the
// query is highlighted.
class QueryHighlighter : public Ranker {
 public:
  QueryHighlighter();
  ~QueryHighlighter() override;

  QueryHighlighter(const QueryHighlighter&) = delete;
  QueryHighlighter& operator=(const QueryHighlighter&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

 private:
  std::u16string last_query_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_QUERY_HIGHLIGHTER_H_
