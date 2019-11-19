// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_H_

#include <memory>

#include "base/callback_forward.h"

namespace chromeos {

class RecommendAppsFetcherDelegate;

class RecommendAppsFetcher {
 public:
  static std::unique_ptr<RecommendAppsFetcher> Create(
      RecommendAppsFetcherDelegate* delegate);

  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<RecommendAppsFetcher>(
          RecommendAppsFetcherDelegate* delegate)>;
  static void SetFactoryCallbackForTesting(FactoryCallback* callback);

  virtual ~RecommendAppsFetcher() = default;

  virtual void Start() = 0;
  virtual void Retry() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_H_
