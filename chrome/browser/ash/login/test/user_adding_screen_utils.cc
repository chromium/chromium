// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/user_adding_screen_utils.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chromeos/ash/experiences/login/login_screen_shown_observer.h"

namespace ash {
namespace test {
namespace {

// A waiter that blocks until the target login screen is reached.
class LoginScreenWaiter : public LoginScreenShownObserver {
 public:
  LoginScreenWaiter() {
    LoginScreenClientImpl::Get()->AddLoginScreenShownObserver(this);
  }
  ~LoginScreenWaiter() override {
    LoginScreenClientImpl::Get()->RemoveLoginScreenShownObserver(this);
  }
  LoginScreenWaiter(const LoginScreenWaiter&) = delete;
  LoginScreenWaiter& operator=(const LoginScreenWaiter&) = delete;

  // LoginScreenShownObserver:
  void OnLoginScreenShown() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

void ShowUserAddingScreen() {
  LoginScreenWaiter waiter;
  UserAddingScreen::Get()->Start();
  waiter.Wait();
}

}  // namespace test
}  // namespace ash
