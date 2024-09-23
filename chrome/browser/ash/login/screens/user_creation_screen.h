// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"

namespace ash {

class UserCreationView;

// Controller for the user creation screen.
class UserCreationScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  enum class Result {
    SIGNIN,
    SIGNIN_TRIAGE,
    ADD_CHILD,
    ENTERPRISE_ENROLL_TRIAGE,
    ENTERPRISE_ENROLL_SHORTCUT,
    CANCEL,
    SKIPPED,
    KIOSK_ENTERPRISE_ENROLL,
    SIGNIN_SCHOOL,
  };

  using TView = UserCreationView;
  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  class UserCreationScreenExitTestDelegate {
   public:
    virtual ~UserCreationScreenExitTestDelegate() = default;

    virtual void OnUserCreationScreenExit(
        Result result,
        const ScreenExitCallback& original_callback) = 0;
  };

  static std::string GetResultString(Result result);

  explicit UserCreationScreen(base::WeakPtr<UserCreationView> view,
                              ErrorScreen* error_screen,
                              const ScreenExitCallback& exit_callback);
  ~UserCreationScreen() override;

  UserCreationScreen(const UserCreationScreen&) = delete;
  UserCreationScreen& operator=(const UserCreationScreen&) = delete;

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

  static void SetUserCreationScreenExitTestDelegate(
      UserCreationScreenExitTestDelegate* test_delegate);

  void SetChildSetupStep();
  void SetDefaultStep();

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // Runs either exit_callback_ or |test_exit_delegate| observer.
  void RunExitCallback(Result result);

  base::WeakPtr<UserCreationView> view_;

  scoped_refptr<NetworkStateInformer> network_state_informer_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  raw_ptr<ErrorScreen> error_screen_ = nullptr;

  // TODO(crbug.com/1154669) Refactor error screen usage
  bool error_screen_visible_ = false;

  // Remember to always use RunExitCallback() above!
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_
