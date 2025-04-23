// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_LAUNCH_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"

class PrefService;

namespace base {
class CommandLine;
}

namespace ash {

// Removes obsolete preferences left out by previous user session;
void ResetEphemeralKioskPreferences(PrefService* prefs);
// Replace the list of preferences which are reset in tests.
void SetEphemeralKioskPreferencesListForTesting(std::vector<std::string>*);

// Checks whether kiosk auto launch should be started.
bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line,
                              const PrefService& local_state);
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_LAUNCH_UTILS_H_
