// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/webui_accelerator_mapping.h"

#include <string>

#include "ash/public/cpp/login_accelerators.h"

namespace chromeos {

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

std::string MapToWebUIAccelerator(ash::LoginAcceleratorAction action) {
  switch (action) {
    case ash::LoginAcceleratorAction::kToggleSystemInfo:
      return kAccelNameVersion;
    case ash::LoginAcceleratorAction::kShowFeedback:
      return kAccelSendFeedback;
    case ash::LoginAcceleratorAction::kShowResetScreen:
      return kAccelNameReset;
    case ash::LoginAcceleratorAction::kAppLaunchBailout:
      return kAccelNameAppLaunchBailout;
    case ash::LoginAcceleratorAction::kAppLaunchNetworkConfig:
      return kAccelNameAppLaunchNetworkConfig;
    case ash::LoginAcceleratorAction::kCancelScreenAction:
      return kAccelNameCancel;
    case ash::LoginAcceleratorAction::kStartEnrollment:
      return kAccelNameEnrollment;
    case ash::LoginAcceleratorAction::kEnableConsumerKiosk:
      return kAccelNameKioskEnable;
    case ash::LoginAcceleratorAction::kEnableDebugging:
      return kAccelNameEnableDebugging;
    case ash::LoginAcceleratorAction::kEditDeviceRequisition:
      return kAccelNameDeviceRequisition;
    case ash::LoginAcceleratorAction::kDeviceRequisitionRemora:
      return kAccelNameDeviceRequisitionRemora;
    case ash::LoginAcceleratorAction::kStartDemoMode:
      return kAccelNameDemoMode;
    case ash::LoginAcceleratorAction::kLaunchDiagnostics:
      return kAccelNameLaunchDiagnostics;
  }
}

}  // namespace chromeos
