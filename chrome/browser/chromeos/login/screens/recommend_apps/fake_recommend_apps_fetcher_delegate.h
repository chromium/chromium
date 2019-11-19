// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_FAKE_RECOMMEND_APPS_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_FAKE_RECOMMEND_APPS_FETCHER_DELEGATE_H_

#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"

#include "base/callback.h"
#include "base/values.h"

namespace chromeos {

// Delegate interface used by RecommendAppsFetcher to report its results.
class FakeRecommendAppsFetcherDelegate : public RecommendAppsFetcherDelegate {
 public:
  // Set of possible results reported by RecommendAppsFetcher.
  enum class Result { UNKNOWN, SUCCESS, LOAD_ERROR, PARSE_ERROR };

  FakeRecommendAppsFetcherDelegate();
  ~FakeRecommendAppsFetcherDelegate() override;

  FakeRecommendAppsFetcherDelegate(
      const FakeRecommendAppsFetcherDelegate& other) = delete;
  FakeRecommendAppsFetcherDelegate& operator=(
      const FakeRecommendAppsFetcherDelegate& other) = delete;

  const base::Value& loaded_apps() const { return loaded_apps_; }
  Result result() const { return result_; }

  // Waits until a result is reported to the delegate, and returns the returned
  // result type.
  Result WaitForResult();

  // Resets the delegate - it clears any previously reported fetcher results.
  void Reset() {
    loaded_apps_ = base::Value();
    result_ = Result::UNKNOWN;
  }

  // RecommendAppsFetcherDelegate:
  void OnLoadSuccess(const base::Value& app_list) override;
  void OnLoadError() override;
  void OnParseResponseError() override;

 private:
  // Records a result value - |loaded_apps_|, if any, should be set before
  // calling this.
  void SetResult(Result result);

  // The last result reported by the RecommendAppsFetcher.
  Result result_ = Result::UNKNOWN;

  // The last reported list of apps reported by the RecommendAppsFetcher. Set
  // only on LoadSuccess.
  base::Value loaded_apps_;

  // The callback that will be called when the result is set - used to implement
  // WaitForResult().
  base::OnceClosure result_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_FAKE_RECOMMEND_APPS_FETCHER_DELEGATE_H_
