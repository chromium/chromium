// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/almanac_api_util.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"

namespace apps {

std::string GetAlmanacApiUrl() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kAlmanacApiUrl)) {
    return command_line->GetSwitchValueASCII(ash::switches::kAlmanacApiUrl);
  }

  return "https://chromeosalmanac-pa.googleapis.com/";
}

}  // namespace apps
