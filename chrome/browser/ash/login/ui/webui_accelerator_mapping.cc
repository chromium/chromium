// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/webui_accelerator_mapping.h"

#include <string>

#include "ash/public/cpp/login_accelerators.h"

namespace ash {
namespace {

// These strings must be kept in sync with handleAccelerator()
// in display_manager.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnableDebugging[] = "debugging";
const char kAccelNameEnrollment[] = "enrollment";
const char kAccelNameKioskEnable[] = "kiosk_enable";
const char kAccelNameVersion[] = "version";
const char kAccelNameReset[] = "reset";
const char kAccelNameDeviceRequisition[] = "device_requisition";
const char kAccelNameDeviceRequisitionRemora[] = "device_requisition_remora";
const char kAccelNameAppLaunchBailout[] = "app_launch_bailout";
const char kAccelNameAppLaunchNetworkConfig[] = "app_launch_network_config";
const char kAccelNameDemoMode[] = "demo_mode";
const char kAccelSendFeedback[] = "send_feedback";
const char kAccelNameLaunchDiagnostics[] = "launch_diagnostics";

}  // namespace

std::string MapToWebUIAccelerator(LoginAcceleratorAction action) {
  switch (action) {
    case LoginAcceleratorAction::kToggleSystemInfo:
      return kAccelNameVersion;
    case LoginAcceleratorAction::kShowFeedback:
      return kAccelSendFeedback;
    case LoginAcceleratorAction::kShowResetScreen:
      return kAccelNameReset;
    case LoginAcceleratorAction::kAppLaunchBailout:
      return kAccelNameAppLaunchBailout;
    case LoginAcceleratorAction::kAppLaunchNetworkConfig:
      return kAccelNameAppLaunchNetworkConfig;
    case LoginAcceleratorAction::kCancelScreenAction:
      return kAccelNameCancel;
    case LoginAcceleratorAction::kStartEnrollment:
      return kAccelNameEnrollment;
    case LoginAcceleratorAction::kEnableConsumerKiosk:
      return kAccelNameKioskEnable;
    case LoginAcceleratorAction::kEnableDebugging:
      return kAccelNameEnableDebugging;
    case LoginAcceleratorAction::kEditDeviceRequisition:
      return kAccelNameDeviceRequisition;
    case LoginAcceleratorAction::kDeviceRequisitionRemora:
      return kAccelNameDeviceRequisitionRemora;
    case LoginAcceleratorAction::kStartDemoMode:
      return kAccelNameDemoMode;
    case LoginAcceleratorAction::kLaunchDiagnostics:
      return kAccelNameLaunchDiagnostics;
  }
}

}  // namespace ash
