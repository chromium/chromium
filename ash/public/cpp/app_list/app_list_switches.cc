// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace ash {
namespace switches {

// Specifies the chrome-extension:// URL for the contents of an additional page
// added to the app launcher.
const char kCustomLauncherPage[] = "custom-launcher-page";

// If set, the app list will not be dismissed when it loses focus. This is
// useful when testing the app list or a custom launcher page. It can still be
// dismissed via the other methods (like the Esc key).
const char kDisableAppListDismissOnBlur[] = "disable-app-list-dismiss-on-blur";

// If set, the app list will be enabled as if enabled from CWS.
const char kEnableAppList[] = "enable-app-list";

// If set, the CrOSActionRecorder will be enabled which will record some user
// actions on device.
const char kEnableCrOSActionRecorder[] = "enable-cros-action-recorder";
// Log user actions with action name hashed.
const char kCrOSActionRecorderWithHash[] = "log-with-hash";
// Log user actions with action name unhashed.
const char kCrOSActionRecorderWithoutHash[] = "log-without-hash";

bool ShouldNotDismissOnBlur() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableAppListDismissOnBlur);
}

}  // namespace switches
}  // namespace ash
