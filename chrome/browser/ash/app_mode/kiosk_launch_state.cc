// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_launch_state.h"

#include <string>

#include "components/crash/core/common/crash_key.h"

namespace ash {

const char kKioskLaunchStateCrashKey[] = "kiosk-launch-state";

std::string KioskLaunchStateToString(KioskLaunchState state) {
  switch (state) {
    case KioskLaunchState::kAttemptToLaunch:
      return "attempt-to-launch";
    case KioskLaunchState::kStartLaunch:
      return "start-launch";
    case KioskLaunchState::kLauncherStarted:
      return "launcher-started";
    case KioskLaunchState::kLaunchFailed:
      return "launch-failed";
    case KioskLaunchState::kAppWindowCreated:
      return "app-window-created";
  }
}

void SetKioskLaunchStateCrashKey(KioskLaunchState state) {
  static crash_reporter::CrashKeyString<32> crash_key(
      kKioskLaunchStateCrashKey);
  crash_key.Set(KioskLaunchStateToString(state));
}

}  // namespace ash
