// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_THEME_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_THEME_SELECTION_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

class ThemeSelectionScreenView;
class WizardContext;

// controller for the OOBE screen with selection of the theme (dark/light)
class ThemeSelectionScreen : public BaseScreen {
 public:
  using TView = ThemeSelectionScreenView;

  enum class Result {
    kProceed,
    kNotApplicable,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class SelectedTheme {
    kAuto = 0,
    kDark = 1,
    kLight = 2,
    kMaxValue = kLight,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  ThemeSelectionScreen(base::WeakPtr<ThemeSelectionScreenView> view,
                       const ScreenExitCallback& exit_callback);

  ThemeSelectionScreen(const ThemeSelectionScreen&) = delete;
  ThemeSelectionScreen& operator=(const ThemeSelectionScreen&) = delete;

  ~ThemeSelectionScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  bool MaybeSkip(WizardContext& context) override;
  bool ShouldBeSkipped(const WizardContext& context) const override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  ScreenSummary GetScreenSummary() override;
  std::string RetrieveChoobeSubtitle();

  ThemeSelectionScreen::SelectedTheme initial_theme_;
  base::WeakPtr<ThemeSelectionScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_THEME_SELECTION_SCREEN_H_
