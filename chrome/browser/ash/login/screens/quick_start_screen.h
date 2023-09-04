// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

namespace ash {

class QuickStartController;

class QuickStartScreen
    : public BaseScreen,
      public quick_start::TargetDeviceBootstrapController::Observer {
 public:
  using TView = QuickStartView;

  // State of the flow when the screen is shown.
  enum class FlowState {
    INITIAL,
    RESUMING_AFTER_CRITICAL_UPDATE,
    CONTINUING_AFTER_ENROLLMENT_CHECKS,
    UNKNOWN,
  };

  enum class EntryPoint {
    WELCOME_SCREEN,
    NETWORK_SCREEN,
    SIGNIN_SCREEN,
  };

  enum class Result {
    // leaving this till the new approach works
    CANCEL_AND_RETURN_TO_WELCOME,
    CANCEL_AND_RETURN_TO_NETWORK,
    CANCEL_AND_RETURN_TO_SIGNIN,
    WIFI_CONNECTED
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  QuickStartScreen(base::WeakPtr<TView> view,
                   QuickStartController* controller,
                   const ScreenExitCallback& exit_callback);

  QuickStartScreen(const QuickStartScreen&) = delete;
  QuickStartScreen& operator=(const QuickStartScreen&) = delete;

  ~QuickStartScreen() override;

  static std::string GetResultString(Result result);

  // Sets the flow state that determines the actions that will be performed when
  // the screen is shown.
  void SetFlowState(FlowState flow_state);

  // Sets the entry point of quick start screen, this is to determine which
  // screen to return to if quick start screen is cancelled.
  void SetEntryPoint(EntryPoint entry_point);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // quick_start::TargetDeviceBootstrapController::Observer:
  void OnStatusChanged(
      const quick_start::TargetDeviceBootstrapController::Status& status) final;

  void OnTransferredGoogleAccountDetails(
      const quick_start::TargetDeviceBootstrapController::Status& status);

  // Sets in the UI the discoverable name that will be used for advertising.
  // Android devices will see this fast pair notification 'Chromebook (123)'
  void DetermineDiscoverableName();

  // Retrieves the connected phone ID and saves it for later use in OOBE on the
  // MultideviceSetupScreen.
  void SavePhoneInstanceID();

  FlowState flow_state_ = FlowState::UNKNOWN;
  EntryPoint entry_point_ = EntryPoint::WELCOME_SCREEN;
  std::string discoverable_name_;
  base::WeakPtr<TView> view_;
  raw_ptr<QuickStartController> controller_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
