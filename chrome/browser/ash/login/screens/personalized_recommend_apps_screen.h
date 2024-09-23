// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PERSONALIZED_RECOMMEND_APPS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PERSONALIZED_RECOMMEND_APPS_SCREEN_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash {
class OOBEAppDefinition;
class OOBEDeviceUseCase;
class PersonalizedRecommendAppsScreenView;
enum class AppsFetchingResult;

// Controller for the new recommended apps screen.
class PersonalizedRecommendAppsScreen : public BaseScreen {
 public:
  using TView = PersonalizedRecommendAppsScreenView;

  enum class Result {
    kNext,
    kSkip,
    kBack,
    kDataMalformed,
    kError,
    kTimeout,
    kNotApplicable
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  PersonalizedRecommendAppsScreen(
      base::WeakPtr<PersonalizedRecommendAppsScreenView> view,
      const ScreenExitCallback& exit_callback);

  PersonalizedRecommendAppsScreen(const PersonalizedRecommendAppsScreen&) =
      delete;
  PersonalizedRecommendAppsScreen& operator=(
      const PersonalizedRecommendAppsScreen&) = delete;

  ~PersonalizedRecommendAppsScreen() override;

  static std::string GetResultString(Result result);

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void set_delay_for_overview_step_for_testing(base::TimeDelta delay) {
    delay_overview_step_ = delay;
  }

  void set_delay_for_set_apps_for_testing(base::TimeDelta delay) {
    delay_set_apps_step_ = delay;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void OnResponseReceived(const std::vector<OOBEAppDefinition>& app_infos,
                          const std::vector<OOBEDeviceUseCase>& use_cases,
                          AppsFetchingResult result);

  void OnInstall(base::Value::List selected_apps_package_ids);

  void ShowOverviewStep();
  // TODO(b/345694992) : Extend browser test to test timeout logic.
  void ExitScreenTimeout();
  void SetAppsAndUseCasesData(base::Value::List apps_with_use_cases_list);

  std::unique_ptr<base::OneShotTimer> delay_set_apps_timer_;
  base::TimeDelta delay_set_apps_step_ = base::Seconds(2);

  std::unique_ptr<base::OneShotTimer> delay_overview_timer_;
  base::TimeDelta delay_overview_step_ = base::Seconds(3);

  // The time when loading UI step started.
  base::TimeTicks loading_start_time_;

  // Amount of apps for each type after filtering.
  std::unordered_map<apps::PackageType, size_t> filtered_apps_count_by_type_;

  // This map is used to store order for each app, so we can record it in UMA
  // for the apps selected for installation.
  std::unordered_map<std::string, int> app_package_id_to_order_;

  std::unique_ptr<base::OneShotTimer> timeout_overview_timer_;
  base::TimeDelta delay_exit_timeout_ = base::Minutes(1);

  base::WeakPtr<PersonalizedRecommendAppsScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<PersonalizedRecommendAppsScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PERSONALIZED_RECOMMEND_APPS_SCREEN_H_
