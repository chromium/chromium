// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_ALLOWLIST_CHECK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_ALLOWLIST_CHECK_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class UserAllowlistCheckScreenView;

// Show error UI at the end of Gaia flow when user is not allowlisted.
class UserAllowlistCheckScreen : public BaseScreen {
 public:
  using TView = UserAllowlistCheckScreenView;

  enum class Result {
    RETRY,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  UserAllowlistCheckScreen(base::WeakPtr<UserAllowlistCheckScreenView> view,
                           const ScreenExitCallback& exit_callback);

  UserAllowlistCheckScreen(const UserAllowlistCheckScreen&) = delete;
  UserAllowlistCheckScreen& operator=(const UserAllowlistCheckScreen&) = delete;

  ~UserAllowlistCheckScreen() override;

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<UserAllowlistCheckScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<UserAllowlistCheckScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_ALLOWLIST_CHECK_SCREEN_H_
