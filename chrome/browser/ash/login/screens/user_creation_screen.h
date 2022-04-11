// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"

namespace ash {

// Controller for the user creation screen.
class UserCreationScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  enum class Result {
    SIGNIN,
    CHILD_SIGNIN,
    CHILD_ACCOUNT_CREATE,
    ENTERPRISE_ENROLL,
    CANCEL,
    SKIPPED,
    KIOSK_ENTERPRISE_ENROLL,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  class UserCreationScreenExitTestDelegate {
   public:
    virtual ~UserCreationScreenExitTestDelegate() = default;

    virtual void OnUserCreationScreenExit(
        Result result,
        const ScreenExitCallback& original_callback) = 0;
  };

  static std::string GetResultString(Result result);

  explicit UserCreationScreen(UserCreationView* view,
                              ErrorScreen* error_screen,
                              const ScreenExitCallback& exit_callback);
  ~UserCreationScreen() override;

  UserCreationScreen(const UserCreationScreen&) = delete;
  UserCreationScreen& operator=(const UserCreationScreen&) = delete;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(UserCreationView* view);

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

  static void SetUserCreationScreenExitTestDelegate(
      UserCreationScreenExitTestDelegate* test_delegate);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // Runs either exit_callback_ or |test_exit_delegate| observer.
  void RunExitCallback(Result result);

  UserCreationView* view_ = nullptr;

  scoped_refptr<NetworkStateInformer> network_state_informer_;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  ErrorScreen* error_screen_ = nullptr;

  // TODO(crbug.com/1154669) Refactor error screen usage
  bool error_screen_visible_ = false;

  // Remember to always use RunExitCallback() above!
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::UserCreationScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_
