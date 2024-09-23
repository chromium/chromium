// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SPLIT_MODIFIER_KEYBOARD_INFO_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SPLIT_MODIFIER_KEYBOARD_INFO_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"

namespace ash {

class SplitModifierKeyboardInfoScreenView;

class SplitModifierKeyboardInfoScreen : public BaseScreen {
 public:
  using TView = SplitModifierKeyboardInfoScreenView;

  enum class Result { kNext = 0, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  SplitModifierKeyboardInfoScreen(
      base::WeakPtr<SplitModifierKeyboardInfoScreenView> view,
      const ScreenExitCallback& exit_callback);

  SplitModifierKeyboardInfoScreen(const SplitModifierKeyboardInfoScreen&) =
      delete;
  SplitModifierKeyboardInfoScreen& operator=(
      const SplitModifierKeyboardInfoScreen&) = delete;

  ~SplitModifierKeyboardInfoScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);
  static bool ShouldBeSkipped();

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<SplitModifierKeyboardInfoScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SPLIT_MODIFIER_KEYBOARD_INFO_SCREEN_H_
