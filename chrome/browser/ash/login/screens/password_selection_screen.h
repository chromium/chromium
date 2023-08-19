// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PASSWORD_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PASSWORD_SELECTION_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class PasswordSelectionScreenView;

// Controller for the Password Selection Screen, which allows the user to choose
// between the local password or Gaia password setup.
class PasswordSelectionScreen : public BaseScreen {
 public:
  using TView = PasswordSelectionScreenView;
  enum class Result {
    NOT_APPLICABLE,
    BACK,
    LOCAL_PASSWORD,
    GAIA_PASSWORD,
  };
  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  PasswordSelectionScreen(base::WeakPtr<PasswordSelectionScreenView> view,
                          ScreenExitCallback exit_callback);
  ~PasswordSelectionScreen() override;

  PasswordSelectionScreen(const PasswordSelectionScreen&) = delete;
  PasswordSelectionScreen& operator=(const PasswordSelectionScreen&) = delete;

  ScreenExitCallback get_exit_callback_for_testing() { return exit_callback_; }
  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool MaybeSkip(WizardContext& context) override;

 private:
  base::WeakPtr<PasswordSelectionScreenView> view_ = nullptr;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<PasswordSelectionScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PASSWORD_SELECTION_SCREEN_H_
