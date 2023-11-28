// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/prefs/pref_service.h"

namespace base {
class Value;
}

namespace ash {
class RecommendAppsScreenView;

// This is Recommend Apps screen that is displayed as a part of user first
// sign-in flow.
class RecommendAppsScreen : public BaseScreen {
 public:
  using TView = RecommendAppsScreenView;

  enum class Result { kSelected, kSkipped, kNotApplicable, kLoadError };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  RecommendAppsScreen(base::WeakPtr<RecommendAppsScreenView> view,
                      const ScreenExitCallback& exit_callback);

  RecommendAppsScreen(const RecommendAppsScreen&) = delete;
  RecommendAppsScreen& operator=(const RecommendAppsScreen&) = delete;

  ~RecommendAppsScreen() override;

  // Called when the user skips the Recommend Apps screen.
  void OnSkip();

  // Called when the user tries to reload the screen.
  void OnRetry();

  // Called when the user Install the selected apps.
  void OnInstall(base::Value::List apps);

  void SetSkipForTesting() { skip_for_testing_ = true; }

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;

  void set_exit_callback_for_testing(ScreenExitCallback exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void OnRecommendationsDownloaded(const std::vector<apps::Result>& result,
                                   apps::DiscoveryError error);
  void UnpackResultAndShow(const std::vector<apps::Result>& result);

  void OnLoadError();
  void OnParseResponseError();

  base::WeakPtr<RecommendAppsScreenView> view_;
  ScreenExitCallback exit_callback_;

  raw_ptr<apps::AppDiscoveryService> app_discovery_service_ = nullptr;

  // Skip the screen for testing if set to true.
  bool skip_for_testing_ = false;

  size_t recommended_app_count_ = 0;

  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<RecommendAppsScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
