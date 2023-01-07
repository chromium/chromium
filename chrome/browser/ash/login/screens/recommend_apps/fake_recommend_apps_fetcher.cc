// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recommend_apps/fake_recommend_apps_fetcher.h"

#include "base/cxx17_backports.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"

namespace ash {
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
  base::Value apps(base::Value::Type::LIST);
  for (int i = 0; i < base::clamp(fake_apps_count_, 0, kMaxAppCount); i++) {
    base::Value app(base::Value::Type::DICTIONARY);
    app.SetKey("name", base::Value(base::StringPrintf("Fake App %d", (i + 1))));
    app.SetKey("package_name", base::Value(base::StringPrintf(
                                   "com.example.fake.app%d", (i + 1))));
    apps.Append(std::move(app));
  }

  delegate_->OnLoadSuccess(std::move(apps));
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

}  // namespace ash
