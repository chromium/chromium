// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_UTIL_H_
#define ASH_GLANCEABLES_GLANCEABLES_UTIL_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace ash::glanceables_util {

// Registers local state prefs for glanceables.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Records the signout screenshot duration in a pref so it can be recorded as an
// UMA metric on the next startup.
ASH_EXPORT void SaveSignoutScreenshotDuration(PrefService* local_state,
                                              base::TimeDelta duration);

// Records an UMA metric for the time the last signout screenshot took. Resets
// the pref used to store the metric across signouts.
ASH_EXPORT void RecordSignoutScreenshotDurationMetric(PrefService* local_state);

// Returns the path to the signout screenshot, for example
// /home/chronos/u-<hash>/signout_screenshot.png
base::FilePath GetSignoutScreenshotPath();

ASH_EXPORT base::TimeDelta GetSignoutScreenshotDurationForTest(
    PrefService* local_state);

}  // namespace ash::glanceables_util

#endif  // ASH_GLANCEABLES_GLANCEABLES_UTIL_H_
