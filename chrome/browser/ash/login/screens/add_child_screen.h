// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ADD_CHILD_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ADD_CHILD_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"

namespace ash {

class AddChildScreenView;

// Controller for the add child screen.
class AddChildScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = AddChildScreenView;

  enum class Result {
    CHILD_SIGNIN,
    CHILD_ACCOUNT_CREATE,
    ENTERPRISE_ENROLL,
    BACK,
    KIOSK_ENTERPRISE_ENROLL,
    SKIPPED,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit AddChildScreen(base::WeakPtr<AddChildScreenView> view,
                          ErrorScreen* error_screen,
                          const ScreenExitCallback& exit_callback);

  AddChildScreen(const AddChildScreen&) = delete;
  AddChildScreen& operator=(const AddChildScreen&) = delete;

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

  ~AddChildScreen() override;

  static std::string GetResultString(Result result);

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  base::WeakPtr<AddChildScreenView> view_;

  scoped_refptr<NetworkStateInformer> network_state_informer_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  raw_ptr<ErrorScreen> error_screen_ = nullptr;

  // TODO(crbug.com/1154669) Refactor error screen usage
  bool error_screen_visible_ = false;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(crbug.com/40163357): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash ::AddChildScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ADD_CHILD_SCREEN_H_
