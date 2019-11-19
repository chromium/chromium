// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps/fake_recommend_apps_fetcher_delegate.h"

#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"

namespace chromeos {

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

void FakeRecommendAppsFetcherDelegate::OnLoadSuccess(
    const base::Value& app_list) {
  loaded_apps_ = app_list.Clone();
  SetResult(Result::SUCCESS);
}

void FakeRecommendAppsFetcherDelegate::SetResult(Result result) {
  DCHECK_EQ(Result::UNKNOWN, result_);
  result_ = result;
  if (result_callback_)
    std::move(result_callback_).Run();
}

}  // namespace chromeos
