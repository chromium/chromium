// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_accelerators.h"

#include <string>

namespace ash {
namespace {

// These strings must be kept in sync with handleAccelerator()
// in display_manager.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameVersion[] = "version";
const char kAccelNameReset[] = "reset";
const char kAccelNameAppLaunchBailout[] = "app_launch_bailout";
const char kAccelNameAppLaunchNetworkConfig[] = "app_launch_network_config";

}  // namespace

// clang-format off
const LoginAcceleratorData kLoginAcceleratorData[] = {
    {
        kToggleSystemInfo,
        ui::VKEY_V, ui::EF_ALT_DOWN,
        true, kScopeOobe | kScopeLogin | kScopeLock,
    }, {
        kShowFeedback,
        ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        true, kScopeOobe | kScopeLogin,
    }, {
        kShowResetScreen,
        ui::VKEY_R, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        true, kScopeOobe | kScopeLogin,
    }, {
       kAppLaunchBailout,
       ui::VKEY_S, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe | kScopeLogin,
    }, {
       kAppLaunchNetworkConfig,
       ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe | kScopeLogin,
    }, {
       kCancelScreenAction,
       ui::VKEY_ESCAPE, ui::EF_NONE,
       false, kScopeOobe | kScopeLogin,
    }, {
       kStartEnrollment,
       ui::VKEY_E, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe,
    }, {
       kStartKioskEnrollment,
       ui::VKEY_K, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe,
    }, {
       kStartDemoMode,
       ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe,
    }, {
       kEnableDebugging,
       ui::VKEY_X, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       false, kScopeOobe,
    }, {
       kEditDeviceRequisition,
       ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       false, kScopeOobe,
    }, {
       kDeviceRequisitionRemora,
       ui::VKEY_H, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe,
    }, {
       kEnableConsumerKiosk,
       ui::VKEY_K, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       false, kScopeOobe,
    }, {
       kLaunchDiagnostics,
       ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
       true, kScopeOobe | kScopeLogin,
    },
};
// clang-format on

const size_t kLoginAcceleratorDataLength = std::size(kLoginAcceleratorData);

std::string MapToWebUIAccelerator(LoginAcceleratorAction action) {
  switch (action) {
    case LoginAcceleratorAction::kToggleSystemInfo:
      return kAccelNameVersion;
    case LoginAcceleratorAction::kShowResetScreen:
      return kAccelNameReset;
    case LoginAcceleratorAction::kAppLaunchBailout:
      return kAccelNameAppLaunchBailout;
    case LoginAcceleratorAction::kAppLaunchNetworkConfig:
      return kAccelNameAppLaunchNetworkConfig;
    case LoginAcceleratorAction::kCancelScreenAction:
      return kAccelNameCancel;
    case LoginAcceleratorAction::kShowFeedback:
    case LoginAcceleratorAction::kStartEnrollment:
    case LoginAcceleratorAction::kStartKioskEnrollment:
    case LoginAcceleratorAction::kEnableConsumerKiosk:
    case LoginAcceleratorAction::kEnableDebugging:
    case LoginAcceleratorAction::kEditDeviceRequisition:
    case LoginAcceleratorAction::kDeviceRequisitionRemora:
    case LoginAcceleratorAction::kStartDemoMode:
    case LoginAcceleratorAction::kLaunchDiagnostics:
      return "";
  }
}

}  // namespace ash
