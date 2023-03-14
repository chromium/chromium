// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/app_mode_utils.h"

#include <stddef.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chrome {

namespace {

// If the device is running in forced app mode, returns the ID of the app for
// which the device is forced in app mode. Otherwise, returns nullopt.
absl::optional<std::string> GetForcedAppModeApp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kForceAppMode) ||
      !command_line->HasSwitch(switches::kAppId)) {
    return absl::nullopt;
  }

  return command_line->GetSwitchValueASCII(switches::kAppId);
}

}  // namespace

bool IsCommandAllowedInAppMode(int command_id, bool is_popup) {
  DCHECK(IsRunningInForcedAppMode());

  constexpr int kAllowed[] = {
      IDC_BACK,
      IDC_DEV_TOOLS,
      IDC_DEV_TOOLS_CONSOLE,
      IDC_DEV_TOOLS_INSPECT,
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

  constexpr int kAllowedPopup[] = {IDC_CLOSE_TAB};

  return base::Contains(kAllowed, command_id) ||
         (is_popup && base::Contains(kAllowedPopup, command_id));
}

bool IsRunningInAppMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kKioskMode) ||
         IsRunningInForcedAppMode();
}

bool IsRunningInForcedAppMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceAppMode);
}

bool IsRunningInForcedAppModeForApp(const std::string& app_id) {
  DCHECK(!app_id.empty());

  absl::optional<std::string> forced_app_mode_app = GetForcedAppModeApp();
  if (!forced_app_mode_app.has_value()) {
    return false;
  }

  return app_id == forced_app_mode_app.value();
}

}  // namespace chrome
