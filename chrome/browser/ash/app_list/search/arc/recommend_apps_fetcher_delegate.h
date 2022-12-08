// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_DELEGATE_H_

namespace base {
class Value;
}  // namespace base

namespace app_list {

// Delegate interface used by RecommendAppsFetcher to report its results.
class RecommendAppsFetcherDelegate {
 public:
  virtual ~RecommendAppsFetcherDelegate() = default;

  // Called when the download of the recommend app list is successful.
  virtual void OnLoadSuccess(const base::Value& app_list) = 0;

  // Called when the download of the recommend app list fails.
  virtual void OnLoadError() = 0;

  // Called when parsing the recommend app list response fails.
  virtual void OnParseResponseError() = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_DELEGATE_H_
