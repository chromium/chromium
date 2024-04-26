// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DISPLAY_SIZE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DISPLAY_SIZE_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

class Profile;

namespace ash {
class DisplaySizeScreenView;

// Controller for the display size screen.
class DisplaySizeScreen : public BaseScreen {
 public:
  using TView = DisplaySizeScreenView;

  enum class Result { kNext, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  static std::string GetResultString(Result result);

  // Updates zoom factor if `kOobeDisplaySizeFactorDeferred` pref is set.
  static void MaybeUpdateZoomFactor(Profile* profile);

  DisplaySizeScreen(base::WeakPtr<DisplaySizeScreenView> view,
                    const ScreenExitCallback& exit_callback);

  DisplaySizeScreen(const DisplaySizeScreen&) = delete;
  DisplaySizeScreen& operator=(const DisplaySizeScreen&) = delete;

  ~DisplaySizeScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen:
  bool ShouldBeSkipped(const WizardContext& context) const override;
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  ScreenSummary GetScreenSummary() override;

  base::WeakPtr<DisplaySizeScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(crbug.com/40163357): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash ::DisplaySizeScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DISPLAY_SIZE_SCREEN_H_
