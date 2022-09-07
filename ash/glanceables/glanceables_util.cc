// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::glanceables_util {

// Pref name for how long the signout screenshot took on the previous signout.
const char kSignoutScreenshotDuration[] = "ash.signout_screenshot.duration";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(kSignoutScreenshotDuration,
                                  base::TimeDelta());
}

void SaveSignoutScreenshotDuration(PrefService* local_state,
                                   base::TimeDelta duration) {
  local_state->SetTimeDelta(kSignoutScreenshotDuration, duration);
}

void RecordSignoutScreenshotDurationMetric(PrefService* local_state) {
  base::TimeDelta duration =
      local_state->GetTimeDelta(kSignoutScreenshotDuration);
  // Don't record the metric if we don't have a value.
  if (duration.is_zero())
    return;
  base::UmaHistogramTimes("Ash.Glanceables.SignoutScreenshotDuration",
                          duration);
  // Reset the pref in case the next signout doesn't record a screenshot.
  local_state->SetTimeDelta(kSignoutScreenshotDuration, base::TimeDelta());
}

base::FilePath GetSignoutScreenshotPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.AppendASCII("signout_screenshot.png");
}

base::TimeDelta GetSignoutScreenshotDurationForTest(PrefService* local_state) {
  return local_state->GetTimeDelta(kSignoutScreenshotDuration);
}

}  // namespace ash::glanceables_util
