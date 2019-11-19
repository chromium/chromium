// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/app_mode_utils.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

namespace {

// If the device is running in forced app mode, returns the ID of the app for
// which the device is forced in app mode. Otherwise, returns nullopt.
base::Optional<std::string> GetForcedAppModeApp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kForceAppMode) ||
      !command_line->HasSwitch(switches::kAppId)) {
    return base::nullopt;
  }

  return command_line->GetSwitchValueASCII(switches::kAppId);
}

}  // namespace

bool IsCommandAllowedInAppMode(int command_id) {
  DCHECK(IsRunningInForcedAppMode());

  const int kAllowed[] = {
      IDC_BACK,
      IDC_FORWARD,
      IDC_RELOAD,
      IDC_CLOSE_FIND_OR_STOP,
      IDC_STOP,
      IDC_RELOAD_BYPASSING_CACHE,
      IDC_RELOAD_CLEARING_CACHE,
      IDC_CUT,
      IDC_COPY,
      IDC_PASTE,
      IDC_ZOOM_PLUS,
      IDC_ZOOM_NORMAL,
      IDC_ZOOM_MINUS,
  };

  for (size_t i = 0; i < base::size(kAllowed); ++i) {
    if (kAllowed[i] == command_id)
      return true;
  }

  return false;
}

bool IsRunningInAppMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kKioskMode) ||
         IsRunningInForcedAppMode();
}

bool IsRunningInForcedAppMode() {
  return GetForcedAppModeApp().has_value() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kForceAndroidAppMode) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kForceWebAppMode);
}

bool IsRunningInForcedAppModeForApp(const std::string& app_id) {
  DCHECK(!app_id.empty());

  base::Optional<std::string> forced_app_mode_app = GetForcedAppModeApp();
  if (!forced_app_mode_app.has_value())
    return false;

  return app_id == forced_app_mode_app.value();
}

}  // namespace chrome
