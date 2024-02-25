// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ONLINE_AUTHENTICATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ONLINE_AUTHENTICATION_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class OnlineAuthenticationScreenView;

class OnlineAuthenticationScreen : public BaseScreen {
 public:
  using TView = OnlineAuthenticationScreenView;

  enum class Result {
    BACK,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  OnlineAuthenticationScreen(base::WeakPtr<OnlineAuthenticationScreenView> view,
                             const ScreenExitCallback& exit_callback);

  OnlineAuthenticationScreen(const OnlineAuthenticationScreen&) = delete;
  OnlineAuthenticationScreen& operator=(const OnlineAuthenticationScreen&) =
      delete;

  ~OnlineAuthenticationScreen() override;

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<OnlineAuthenticationScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<OnlineAuthenticationScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ONLINE_AUTHENTICATION_SCREEN_H_
