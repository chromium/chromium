// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_DELEGATE_H_

namespace base {
class Value;
}

namespace chromeos {

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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_DELEGATE_H_
