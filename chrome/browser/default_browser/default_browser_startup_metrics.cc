// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_startup_metrics.h"

#include <windows.h>

#include <algorithm>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/installer/util/beacons.h"

namespace default_browser {

DefaultBrowserBeacons ReadDefaultBrowserBeacons() {
  return {
      .last_was_default = installer_util::MakeLastWasDefaultBeacon()->Get(),
      .first_not_default = installer_util::MakeFirstNotDefaultBeacon()->Get()};
}

// Beacons are also updated by setup.exe (background updates, OS upgrade, Active
// Setup) while Chrome is not running, so a loss first observed there is
// classified as kRemainedNotDefault: kLostDefault is a lower bound.
// LastWasDefault also counts OTHER_MODE_IS_DEFAULT as default.
StartupStateTransition ClassifyStartupTransition(
    const DefaultBrowserBeacons& beacons,
    base::Time session_start,
    shell_integration::DefaultWebClientState current) {
  switch (current) {
    case shell_integration::UNKNOWN_DEFAULT:
      return StartupStateTransition::kCurrentStateUnknown;
    case shell_integration::OTHER_MODE_IS_DEFAULT:
      return StartupStateTransition::kOtherModeIsDefault;
    case shell_integration::IS_DEFAULT:
    case shell_integration::NOT_DEFAULT:
      break;
    case shell_integration::NUM_DEFAULT_STATES:
      NOTREACHED();
  }

  const bool previously_not_default = !beacons.first_not_default.is_null() &&
                                      beacons.first_not_default < session_start;
  const bool previously_default =
      !previously_not_default && !beacons.last_was_default.is_null();
  if (!previously_default && !previously_not_default) {
    return StartupStateTransition::kNoPreviousState;
  }

  const bool is_default = current == shell_integration::IS_DEFAULT;
  if (previously_default) {
    return is_default ? StartupStateTransition::kRemainedDefault
                      : StartupStateTransition::kLostDefault;
  }
  return is_default ? StartupStateTransition::kBecameDefault
                    : StartupStateTransition::kRemainedNotDefault;
}

void RecordStartupStateTransition(
    const DefaultBrowserBeacons& beacons,
    shell_integration::DefaultWebClientState current) {
  const StartupStateTransition transition = ClassifyStartupTransition(
      beacons, base::Process::Current().CreationTime(), current);
  RecordStartupStateTransition(
      transition,
      transition == StartupStateTransition::kLostDefault
          ? GetDefaultBrowserChangeTime()
          : base::Time(),
      base::Time::Now());
}

void RecordStartupStateTransition(StartupStateTransition transition,
                                  base::Time change_time,
                                  base::Time now) {
  base::UmaHistogramEnumeration("DefaultBrowser.StartupStateTransition",
                                transition);
  if (transition == StartupStateTransition::kLostDefault &&
      !change_time.is_null()) {
    const int days = std::max(0, (now - change_time).InDays());
    base::UmaHistogramCustomCounts(
        "DefaultBrowser.StartupLostDefault.DaysSinceChange", days, 1, 366, 50);
  }
}

base::Time GetDefaultBrowserChangeTime() {
  base::Time change_time;
  for (std::wstring_view path : kUserChoiceKeyPaths) {
    base::win::RegKey key;
    if (key.Open(HKEY_CURRENT_USER, path.data(), KEY_QUERY_VALUE) !=
        ERROR_SUCCESS) {
      continue;
    }
    FILETIME last_write_time = {};
    if (::RegQueryInfoKeyW(key.Handle(), nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           &last_write_time) != ERROR_SUCCESS) {
      continue;
    }
    change_time =
        std::max(change_time, base::Time::FromFileTime(last_write_time));
  }
  return change_time;
}

}  // namespace default_browser
