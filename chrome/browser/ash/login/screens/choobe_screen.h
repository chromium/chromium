// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CHOOBE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CHOOBE_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace base {
class Value;
}

namespace ash {
class ChoobeScreenView;

// Controller for the CHOOBE screen.
// Screen displays optional screens and allows
// user to select which screens to be shown.
class ChoobeScreen : public BaseScreen {
 public:
  using TView = ChoobeScreenView;

  enum class Result { SELECTED, SKIPPED, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  ChoobeScreen(base::WeakPtr<ChoobeScreenView> view,
               const ScreenExitCallback& exit_callback);

  ChoobeScreen(const ChoobeScreen&) = delete;
  ChoobeScreen& operator=(const ChoobeScreen&) = delete;

  ~ChoobeScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Called when the user skips the CHOOBE screen.
  void SkipScreen();

  // Called when the user selects screens on the CHOOBE screen.
  void OnSelect(base::Value::List screens);

  base::WeakPtr<ChoobeScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CHOOBE_SCREEN_H_
