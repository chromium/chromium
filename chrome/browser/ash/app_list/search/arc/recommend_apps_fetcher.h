// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_H_

#include <memory>

namespace app_list {

class RecommendAppsFetcherDelegate;

class RecommendAppsFetcher {
 public:
  static std::unique_ptr<RecommendAppsFetcher> Create(
      RecommendAppsFetcherDelegate* delegate);

  virtual ~RecommendAppsFetcher() = default;

  virtual void StartDownload() = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_H_
