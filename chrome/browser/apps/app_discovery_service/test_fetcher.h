// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_TEST_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_TEST_FETCHER_H_

#include "base/callback_list.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

namespace apps {

class Result;

class TestFetcher : public AppFetcher {
 public:
  TestFetcher();
  TestFetcher(const TestFetcher&) = delete;
  TestFetcher& operator=(const TestFetcher&) = delete;
  ~TestFetcher() override;

  void SetResults(std::vector<Result> results);

  // AppFetcher:
  void GetApps(ResultCallback callback) override;
  base::CallbackListSubscription RegisterForAppUpdates(
      RepeatingResultCallback callback) override;

 private:
  ResultCallbackList result_callback_list_;
  std::vector<apps::Result> results_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_TEST_FETCHER_H_
