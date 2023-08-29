// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/fake_recommend_apps_fetcher_delegate.h"

#include <utility>

#include "base/check_op.h"
#include "base/run_loop.h"

namespace apps {

FakeRecommendAppsFetcherDelegate::FakeRecommendAppsFetcherDelegate() = default;

FakeRecommendAppsFetcherDelegate::~FakeRecommendAppsFetcherDelegate() = default;

FakeRecommendAppsFetcherDelegate::Result
FakeRecommendAppsFetcherDelegate::WaitForResult() {
  if (result_ == Result::UNKNOWN) {
    base::RunLoop run_loop;
    result_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  return result_;
}

void FakeRecommendAppsFetcherDelegate::OnLoadError() {
  SetResult(Result::LOAD_ERROR);
}

void FakeRecommendAppsFetcherDelegate::OnParseResponseError() {
  SetResult(Result::PARSE_ERROR);
}

void FakeRecommendAppsFetcherDelegate::OnLoadSuccess(base::Value app_list) {
  loaded_apps_ = std::move(app_list);
  SetResult(Result::SUCCESS);
}

void FakeRecommendAppsFetcherDelegate::SetResult(Result result) {
  DCHECK_EQ(Result::UNKNOWN, result_);
  result_ = result;
  if (result_callback_) {
    std::move(result_callback_).Run();
  }
}

}  // namespace apps
