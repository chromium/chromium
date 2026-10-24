// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_STARTUP_METRICS_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_STARTUP_METRICS_H_

#include <array>
#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/shell_integration.h"

namespace default_browser {

// Keys Windows re-creates when the http/https default changes; the default
// check requires both.
inline constexpr std::array<std::wstring_view, 2> kUserChoiceKeyPaths = {
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\htt"
    L"p\\UserChoice",
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\htt"
    L"ps\\UserChoice"};

// Transition between the default-browser state implied by the installer
// beacons and the one observed at this startup.
//
// LINT.IfChange(DefaultBrowserStartupStateTransition)
enum class StartupStateTransition {
  kNoPreviousState = 0,
  kRemainedDefault = 1,
  kRemainedNotDefault = 2,
  kLostDefault = 3,
  kBecameDefault = 4,
  kCurrentStateUnknown = 5,
  kOtherModeIsDefault = 6,
  kMaxValue = kOtherModeIsDefault,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserStartupStateTransition)

// Default-browser beacons (chrome/installer/util/beacons.h); null Time when
// absent.
struct DefaultBrowserBeacons {
  base::Time last_was_default;
  base::Time first_not_default;
};

// Must be read before the default-browser check, which updates the beacons.
DefaultBrowserBeacons ReadDefaultBrowserBeacons();

// Classifies the transition from the state implied by `beacons` to `current`.
// A FirstNotDefault beacon written after `session_start` was set by an earlier
// check in this process, so the previous state was default.
StartupStateTransition ClassifyStartupTransition(
    const DefaultBrowserBeacons& beacons,
    base::Time session_start,
    shell_integration::DefaultWebClientState current);

// Classifies against the process creation time and reads the change time only
// for kLostDefault.
void RecordStartupStateTransition(
    const DefaultBrowserBeacons& beacons,
    shell_integration::DefaultWebClientState current);

// Records DefaultBrowser.StartupStateTransition and, for kLostDefault with a
// non-null `change_time`, DefaultBrowser.StartupLostDefault.DaysSinceChange.
void RecordStartupStateTransition(StartupStateTransition transition,
                                  base::Time change_time,
                                  base::Time now);

// Latest last-write time of kUserChoiceKeyPaths. Blocking; null if none is
// readable.
base::Time GetDefaultBrowserChangeTime();

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_STARTUP_METRICS_H_
