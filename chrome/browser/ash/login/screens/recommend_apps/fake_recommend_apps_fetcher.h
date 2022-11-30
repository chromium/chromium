// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_FAKE_RECOMMEND_APPS_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_FAKE_RECOMMEND_APPS_FETCHER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher.h"

namespace ash {
class RecommendAppsFetcherDelegate;

// This class fakes network request for the Recommend Apps screen.
// It returns fixed number of fake apps after small delay.
class FakeRecommendAppsFetcher : public RecommendAppsFetcher {
 public:
  FakeRecommendAppsFetcher(RecommendAppsFetcherDelegate* delegate,
                           int fake_apps_count);
  FakeRecommendAppsFetcher(const FakeRecommendAppsFetcher&) = delete;
  FakeRecommendAppsFetcher& operator=(const FakeRecommendAppsFetcher&) = delete;
  ~FakeRecommendAppsFetcher() override;

  // Provide a retry method to download the app list again.
  // RecommendAppsFetcher:
  void Start() override;
  void Retry() override;

 private:
  void OnTimer();

  RecommendAppsFetcherDelegate* delegate_;
  int fake_apps_count_;
  base::OneShotTimer delay_timer_;

  base::WeakPtrFactory<FakeRecommendAppsFetcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_FAKE_RECOMMEND_APPS_FETCHER_H_
