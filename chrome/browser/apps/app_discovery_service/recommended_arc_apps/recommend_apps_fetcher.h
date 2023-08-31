// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_H_

#include <memory>

#include "base/functional/callback_forward.h"

class Profile;

namespace apps {
class RecommendAppsFetcherDelegate;

class RecommendAppsFetcher {
 public:
  static std::unique_ptr<RecommendAppsFetcher> Create(
      Profile* profile,
      RecommendAppsFetcherDelegate* delegate);

  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<RecommendAppsFetcher>(
          RecommendAppsFetcherDelegate* delegate)>;
  static void SetFactoryCallbackForTesting(FactoryCallback* callback);

  virtual ~RecommendAppsFetcher() = default;

  virtual void Start() = 0;
  virtual void Retry() = 0;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_H_
