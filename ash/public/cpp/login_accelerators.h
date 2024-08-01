// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_ACCELERATORS_H_
#define ASH_PUBLIC_CPP_LOGIN_ACCELERATORS_H_

#include <stddef.h>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// Accelerator actions specific for out-of-box flow, login and lock screens.

// Flags that define in which contexts accelerator should be enabled.
enum LoginActionScope {
  // Available during out-of-box flow.
  kScopeOobe = 1 << 0,
  // Available on the login screen.
  kScopeLogin = 1 << 1,
  // Available on the lock screen.
  kScopeLock = 1 << 2,
};

enum LoginAcceleratorAction {
  kToggleSystemInfo,
  kShowFeedback,
  kShowResetScreen,
  kAppLaunchBailout,
  kAppLaunchNetworkConfig,
  kCancelScreenAction,
  kStartEnrollment,
  kStartKioskEnrollment,
  kEnableDebugging,
  kEditDeviceRequisition,
  kDeviceRequisitionRemora,
  kStartDemoMode,
  kLaunchDiagnostics,
  kEnableQuickStart,
};

struct LoginAcceleratorData {
  LoginAcceleratorAction action;
  ui::KeyboardCode keycode;
  // Combination of ui::EventFlags.
  int modifiers;
  // Defines if accelerator will be registered in AcceleratorController (|true|)
  // or only for login/lock dialog view (|false|).
  bool global;
  // Combination of LoginActionScope flags.
  int scope;
};

// Accelerators handled by OOBE / Login components.
ASH_PUBLIC_EXPORT extern const LoginAcceleratorData kLoginAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t kLoginAcceleratorDataLength;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_ACCELERATORS_H_
