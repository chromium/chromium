// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_reporting_util.h"

#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace crostini {

namespace {
void WriteMetricsForReportingToPrefs(
    PrefService* profile_prefs,
    const component_updater::ComponentUpdateService* update_service,
    const base::Clock* clock) {
  const base::Time last_launch_time_window_start =
      GetThreeDayWindowStart(clock->Now());
  const std::string crostini_version = GetTerminaVersion(update_service);
  if (crostini_version.empty()) {
    LOG(ERROR) << "Could not determine Termina version for usage reporting";
  }

  profile_prefs->SetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart,
      last_launch_time_window_start.InMillisecondsSinceUnixEpoch());
  profile_prefs->SetString(
      crostini::prefs::kCrostiniLastLaunchTerminaComponentVersion,
      crostini_version);
}
}  // namespace

void WriteMetricsForReportingToPrefsIfEnabled(
    PrefService* profile_prefs,
    const component_updater::ComponentUpdateService* update_service,
    const base::Clock* clock) {
  if (profile_prefs->GetBoolean(crostini::prefs::kReportCrostiniUsageEnabled)) {
    WriteMetricsForReportingToPrefs(profile_prefs, update_service, clock);
  }
}

void WriteTerminaVmKernelVersionToPrefsForReporting(
    PrefService* profile_prefs,
    const std::string& kernel_version) {
  profile_prefs->SetString(
      crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion, kernel_version);
}

base::Time GetThreeDayWindowStart(const base::Time& actual_time) {
  const base::Time actual_time_midnight = actual_time.UTCMidnight();
  const base::TimeDelta delta =
      base::Days((actual_time_midnight - base::Time::UnixEpoch()).InDays() % 3);
  return actual_time_midnight - delta;
}

std::string GetTerminaVersion(
    const component_updater::ComponentUpdateService* update_service) {
  // Define a UTF16 version of imageloader::kTerminaComponentName for the string
  // comparison below. Assert that it is equal to the ASCII component name.
  static constexpr char16_t kTerminaComponentName16[] = u"cros-termina";
  DCHECK(base::ranges::equal(imageloader::kTerminaComponentName,
                             kTerminaComponentName16));

  const std::vector<component_updater::ComponentInfo> component_list =
      update_service->GetComponents();

  for (const auto& component : component_list) {
    if (component.name == kTerminaComponentName16) {
      return component.version.GetString();
    }
  }

  return std::string();
}

}  // namespace crostini
