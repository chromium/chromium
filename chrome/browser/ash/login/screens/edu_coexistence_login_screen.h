// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_EDU_COEXISTENCE_LOGIN_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_EDU_COEXISTENCE_LOGIN_SCREEN_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog_onboarding.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class ScopedSessionRefresher;
class ScreenManager;

// OOBE screen to add EDU account as a secondary account when the user is a
// supervised user.
class EduCoexistenceLoginScreen : public BaseScreen,
                                  public LoginDisplayHost::Observer {
 public:
  constexpr static StaticOobeScreenId kScreenId{"edu-coexistence-login"};

  enum class Result { DONE, SKIPPED };

  static EduCoexistenceLoginScreen* Get(ScreenManager* screen_manager);
  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;
  explicit EduCoexistenceLoginScreen(const ScreenExitCallback& exit_callback);
  ~EduCoexistenceLoginScreen() override;

  EduCoexistenceLoginScreen(const EduCoexistenceLoginScreen&) = delete;
  EduCoexistenceLoginScreen& operator=(const EduCoexistenceLoginScreen&) =
      delete;

  ScreenExitCallback get_exit_callback_for_testing() { return exit_callback_; }
  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 private:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;

  // LoginDisplayHost::Observer
  void WebDialogViewBoundsChanged(const gfx::Rect& bounds) override;

  ScreenExitCallback exit_callback_;
  std::unique_ptr<InlineLoginDialogOnboarding::Delegate> dialog_delegate_;

  // Keeps cryptohome authsession alive.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  base::ScopedObservation<LoginDisplayHost, LoginDisplayHost::Observer>
      observed_login_display_host_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_EDU_COEXISTENCE_LOGIN_SCREEN_H_
