// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PLACEHOLDER_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PLACEHOLDER_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"

namespace ash {

class PlaceholderScreenView;

class PlaceholderScreen : public BaseScreen {
 public:
  using TView = PlaceholderScreenView;

  enum class Result {
    kNext = 0,
    kBack,
    kNotApplicable
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  PlaceholderScreen(base::WeakPtr<PlaceholderScreenView> view,
                    const ScreenExitCallback& exit_callback);

  PlaceholderScreen(const PlaceholderScreen&) = delete;
  PlaceholderScreen& operator=(const PlaceholderScreen&) = delete;

  ~PlaceholderScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<PlaceholderScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PLACEHOLDER_SCREEN_H_
