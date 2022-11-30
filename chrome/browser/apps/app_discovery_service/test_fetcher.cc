// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/test_fetcher.h"

#include <utility>

#include "chrome/browser/apps/app_discovery_service/result.h"

namespace apps {

TestFetcher::TestFetcher() = default;

TestFetcher::~TestFetcher() = default;

void TestFetcher::SetResults(std::vector<Result> results) {
  results_ = std::move(results);

  result_callback_list_.Notify(results_);
}

void TestFetcher::GetApps(ResultCallback callback) {
  if (!results_.empty()) {
    std::move(callback).Run(std::move(results_), DiscoveryError::kSuccess);
    return;
  }
}

base::CallbackListSubscription TestFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  return result_callback_list_.Add(std::move(callback));
}

}  // namespace apps
