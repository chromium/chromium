// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_FAKE_RECOMMEND_APPS_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_FAKE_RECOMMEND_APPS_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"

namespace apps {
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

  raw_ptr<RecommendAppsFetcherDelegate> delegate_;
  int fake_apps_count_;
  base::OneShotTimer delay_timer_;

  base::WeakPtrFactory<FakeRecommendAppsFetcher> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_FAKE_RECOMMEND_APPS_FETCHER_H_
