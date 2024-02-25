// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_DELEGATE_H_

namespace base {
class Value;
}

namespace apps {

// Delegate interface used by RecommendAppsFetcher to report its results.
class RecommendAppsFetcherDelegate {
 public:
  virtual ~RecommendAppsFetcherDelegate() = default;

  // Called when the download of the recommend app list is successful.
  virtual void OnLoadSuccess(base::Value app_list) = 0;

  // Called when the download of the recommend app list fails.
  virtual void OnLoadError() = 0;

  // Called when parsing the recommend app list response fails.
  virtual void OnParseResponseError() = 0;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_DELEGATE_H_
