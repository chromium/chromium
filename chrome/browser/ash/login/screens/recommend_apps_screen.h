// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"

namespace base {
class Value;
}

namespace ash {
class RecommendAppsFetcher;

// This is Recommend Apps screen that is displayed as a part of user first
// sign-in flow.
class RecommendAppsScreen : public BaseScreen,
                            public RecommendAppsFetcherDelegate {
 public:
  using TView = RecommendAppsScreenView;

  enum class Result { SELECTED, SKIPPED, NOT_APPLICABLE, LOAD_ERROR };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  RecommendAppsScreen(RecommendAppsScreenView* view,
                      const ScreenExitCallback& exit_callback);

  RecommendAppsScreen(const RecommendAppsScreen&) = delete;
  RecommendAppsScreen& operator=(const RecommendAppsScreen&) = delete;

  ~RecommendAppsScreen() override;

  // Called when the user skips the Recommend Apps screen.
  void OnSkip();

  // Called when the user tries to reload the screen.
  void OnRetry();

  // Called when the user Install the selected apps.
  void OnInstall();

  // Called when the view is destroyed so there is no dead reference to it.
  void OnViewDestroyed(RecommendAppsScreenView* view);

  void SetSkipForTesting() { skip_for_testing_ = true; }

  // RecommendAppsFetcherDelegate:
  void OnLoadSuccess(base::Value app_list) override;
  void OnLoadError() override;
  void OnParseResponseError() override;

  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;

  void set_exit_callback_for_testing(ScreenExitCallback exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  RecommendAppsScreenView* view_;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<RecommendAppsFetcher> recommend_apps_fetcher_;

  // Skip the screen for testing if set to true.
  bool skip_for_testing_ = false;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash ::RecommendAppsScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
