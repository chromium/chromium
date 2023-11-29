// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PARENTAL_HANDOFF_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PARENTAL_HANDOFF_SCREEN_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class ParentalHandoffScreenView;

class ParentalHandoffScreen : public BaseScreen {
 public:
  using TView = ParentalHandoffScreenView;

  enum class Result { kDone, kSkipped };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  ParentalHandoffScreen(base::WeakPtr<ParentalHandoffScreenView> view,
                        const ScreenExitCallback& exit_callback);
  ParentalHandoffScreen(const ParentalHandoffScreen&) = delete;
  ParentalHandoffScreen& operator=(const ParentalHandoffScreen&) = delete;
  ~ParentalHandoffScreen() override;

  ScreenExitCallback get_exit_callback_for_test() { return exit_callback_; }

  void set_exit_callback_for_test(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<ParentalHandoffScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PARENTAL_HANDOFF_SCREEN_H_
