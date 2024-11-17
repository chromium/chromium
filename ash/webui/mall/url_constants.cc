// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/mall/url_constants.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chromeos/constants/url_constants.h"
#include "url/gurl.h"

namespace ash {

GURL GetMallBaseUrl() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kMallUrl)) {
    return GURL(command_line->GetSwitchValueASCII(ash::switches::kMallUrl));
  }

  return GURL(chromeos::kAppMallBaseUrl);
}

}  // namespace ash
