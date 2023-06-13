// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

namespace ash {

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

  enum class Result { CANCEL, WIFI_CONNECTED };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  QuickStartScreen(base::WeakPtr<TView> view,
                   const ScreenExitCallback& exit_callback);

  QuickStartScreen(const QuickStartScreen&) = delete;
  QuickStartScreen& operator=(const QuickStartScreen&) = delete;

  ~QuickStartScreen() override;

  static std::string GetResultString(Result result);

  // Sets the flow state that determines the actions that will be performed when
  // the screen is shown.
  void SetFlowState(FlowState flow_state);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // quick_start::TargetDeviceBootstrapController::Observer:
  void OnStatusChanged(
      const quick_start::TargetDeviceBootstrapController::Status& status) final;

  // Sets in the UI the discoverable name that will be used for advertising.
  // Android devices will see this fast pair notification 'Chromebook (123)'
  void DetermineDiscoverableName();

  void UnbindFromBootstrapController();

  // Retrieves the connected phone ID and saves it for later use in OOBE on the
  // MultideviceSetupScreen.
  void SavePhoneInstanceID();

  FlowState flow_state_ = FlowState::UNKNOWN;
  std::string discoverable_name_;
  base::WeakPtr<TView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtr<quick_start::TargetDeviceBootstrapController>
      bootstrap_controller_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
