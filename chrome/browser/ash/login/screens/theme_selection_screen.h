// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_THEME_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_THEME_SELECTION_SCREEN_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/theme_selection_screen_handler.h"

namespace ash {

class WizardContext;

// controller for the OOBE screen with selection of the theme (dark/light)
class ThemeSelectionScreen : public BaseScreen {
 public:
  using TView = ThemeSelectionScreenView;

  enum class Result {
    kProceed,
    kNotApplicable,
  };

  enum class SelectedTheme {
    kAuto = 0,
    kDark = 1,
    kLight = 2,
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

 private:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<ThemeSelectionScreenView> view_;
  ScreenExitCallback exit_callback_;
};
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::ThemeSelectionScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_THEME_SELECTION_SCREEN_H_
