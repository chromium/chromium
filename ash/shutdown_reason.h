// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHUTDOWN_REASON_H_
#define ASH_SHUTDOWN_REASON_H_

namespace ash {

enum class ShutdownReason {
  POWER_BUTTON,            // User pressed the (physical) power button.
  LOGIN_SHUT_DOWN_BUTTON,  // User pressed the login screen shut down button.
  TRAY_SHUT_DOWN_BUTTON,   // User pressed the tray shut down button.
  ARC_POWER_BUTTON,        // ARC power button is invoked.
  DEBUG_ACCELERATOR,       // Power menu debug accelerator
                           // (DEBUG_TOGGLE_POWER_BUTTON_MENU) is pressed.
};

// Returns a string describing |reason|.
const char* ShutdownReasonToString(ShutdownReason reason);

}  // namespace ash

#endif  // ASH_SHUTDOWN_REASON_H_
