// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_accelerators.h"

#include <string>

namespace ash {

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
    },{
        kShowFeedback,
        ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
        true, kScopeOobe | kScopeLogin,
    }, {
        kShowResetScreen,
        ui::VKEY_R, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        true, kScopeOobe | kScopeLogin,
    }, {
       kAppLaunchBailout,
       ui::VKEY_S, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       true, kScopeOobe | kScopeLogin,
    }, {
       kAppLaunchNetworkConfig,
       ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       true, kScopeOobe | kScopeLogin,
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
       kLaunchDiagnostics,
       ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
       true, kScopeOobe | kScopeLogin,
    }, {
      kEnableQuickStart,
      ui::VKEY_Q, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       false, kScopeOobe,
    },
};
// clang-format on

const size_t kLoginAcceleratorDataLength = std::size(kLoginAcceleratorData);

}  // namespace ash
