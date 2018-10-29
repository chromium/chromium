// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace crostini {
namespace prefs {

// A boolean preference representing whether a user has opted in to use
// Crostini (Called "Linux Apps" in UI).
const char kCrostiniEnabled[] = "crostini.enabled";
const char kCrostiniMimeTypes[] = "crostini.mime_types";
const char kCrostiniRegistry[] = "crostini.registry";
// List of filesystem paths that are shared with the crostini container.
const char kCrostiniSharedPaths[] = "crostini.shared_paths";
// A boolean preference representing a user level enterprise policy to enable
// Crostini use.
const char kUserCrostiniAllowedByPolicy[] = "crostini.user_allowed_by_policy";

// A boolean preference controlling Crostini usage reporting.
const char kReportCrostiniUsageEnabled[] = "crostini.usage_reporting_enabled";
// Preferences used to store last launch information for reporting:
const char kCrostiniLastLaunchVersion[] = "crostini.last_launch.version";
// The start of a three day window of the last app launch
// stored as Java time (ms since epoch).
const char kCrostiniLastLaunchTimeWindowStart[] =
    "crostini.last_launch.time_window_start";
// The value of the last sample of the disk space used by Crostini.
const char kCrostiniLastDiskSize[] = "crostini.last_disk_size";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kCrostiniEnabled, false);
  registry->RegisterDictionaryPref(kCrostiniMimeTypes);
  registry->RegisterDictionaryPref(kCrostiniRegistry);
  registry->RegisterListPref(kCrostiniSharedPaths);
  registry->RegisterBooleanPref(crostini::prefs::kReportCrostiniUsageEnabled,
                                false);
  registry->RegisterStringPref(kCrostiniLastLaunchVersion, std::string());
  registry->RegisterInt64Pref(kCrostiniLastLaunchTimeWindowStart, 0u);
  registry->RegisterInt64Pref(kCrostiniLastDiskSize, 0u);
  registry->RegisterBooleanPref(kUserCrostiniAllowedByPolicy, true);
}

}  // namespace prefs
}  // namespace crostini
