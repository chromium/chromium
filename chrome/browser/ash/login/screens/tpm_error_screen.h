// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_TPM_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_TPM_ERROR_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class TpmErrorView;

// Controller for the tpm error screen.
class TpmErrorScreen : public BaseScreen {
 public:
  using TView = TpmErrorView;

  enum class Result { kSkip };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit TpmErrorScreen(base::WeakPtr<TpmErrorView> view,
                          const ScreenExitCallback& exit_callback);
  TpmErrorScreen(const TpmErrorScreen&) = delete;
  TpmErrorScreen& operator=(const TpmErrorScreen&) = delete;
  ~TpmErrorScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<TpmErrorView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_TPM_ERROR_SCREEN_H_
