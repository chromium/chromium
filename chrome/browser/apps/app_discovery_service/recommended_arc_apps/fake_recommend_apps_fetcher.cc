// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/fake_recommend_apps_fetcher.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_delegate.h"

namespace apps {
namespace {

constexpr base::TimeDelta kFakeLoadingTime = base::Seconds(3);
constexpr const int kMaxAppCount = 21;

}  // namespace

FakeRecommendAppsFetcher::FakeRecommendAppsFetcher(
    RecommendAppsFetcherDelegate* delegate,
    int fake_apps_count)
    : delegate_(delegate), fake_apps_count_(fake_apps_count) {}

FakeRecommendAppsFetcher::~FakeRecommendAppsFetcher() = default;

void FakeRecommendAppsFetcher::OnTimer() {
  base::Value::List apps;
  for (int i = 0; i < std::clamp(fake_apps_count_, 0, kMaxAppCount); i++) {
    base::Value::Dict app;
    app.Set("name", base::StringPrintf("Fake App %d", (i + 1)));
    app.Set("package_name",
            base::StringPrintf("com.example.fake.app%d", (i + 1)));
    apps.Append(std::move(app));
  }

  delegate_->OnLoadSuccess(base::Value(std::move(apps)));
}

void FakeRecommendAppsFetcher::Start() {
  delay_timer_.Start(FROM_HERE, kFakeLoadingTime,
                     base::BindOnce(&FakeRecommendAppsFetcher::OnTimer,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void FakeRecommendAppsFetcher::Retry() {
  delay_timer_.Start(FROM_HERE, kFakeLoadingTime,
                     base::BindOnce(&FakeRecommendAppsFetcher::OnTimer,
                                    weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace apps
