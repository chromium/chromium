// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CATEGORIES_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CATEGORIES_SELECTION_SCREEN_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {
enum class AppsFetchingResult;
class CategoriesSelectionScreenView;
class OOBEAppDefinition;
class OOBEDeviceUseCase;

// Controller for the categories selection screen.
class CategoriesSelectionScreen : public BaseScreen {
 public:
  using TView = CategoriesSelectionScreenView;

  enum class Result {
    kNext,
    kSkip,
    kError,
    kDataMalformed,
    kTimeout,
    kNotApplicable
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  CategoriesSelectionScreen(base::WeakPtr<CategoriesSelectionScreenView> view,
                            const ScreenExitCallback& exit_callback);

  CategoriesSelectionScreen(const CategoriesSelectionScreen&) = delete;
  CategoriesSelectionScreen& operator=(const CategoriesSelectionScreen&) =
      delete;

  ~CategoriesSelectionScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void set_delay_for_overview_step_for_testing(base::TimeDelta delay) {
    delay_overview_step_ = delay;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void OnResponseReceived(const std::vector<OOBEAppDefinition>& appInfos,
                          const std::vector<OOBEDeviceUseCase>& categories,
                          AppsFetchingResult result);

  void ShowOverviewStep();
  // TODO(b/345694992) : Extend browser test to test timeout logic.
  void ExitScreenTimeout();

  // Called when the user selects categories on the screen.
  void OnSelect(base::Value::List selected_use_cases_ids);

  std::unique_ptr<base::OneShotTimer> delay_overview_timer_;
  base::TimeDelta delay_overview_step_ = base::Seconds(2);

  std::unique_ptr<base::OneShotTimer> timeout_overview_timer_;
  base::TimeDelta delay_exit_timeout_ = base::Minutes(1);

  // The time when loading UI step started.
  base::TimeTicks loading_start_time_;

  size_t use_cases_total_count_ = 0;

  // This map is used to store order for each use_case, so we can record it in
  // UMA for the selected use-cases.
  std::unordered_map<std::string, int> use_case_id_to_order_;

  base::WeakPtr<CategoriesSelectionScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<CategoriesSelectionScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CATEGORIES_SELECTION_SCREEN_H_
