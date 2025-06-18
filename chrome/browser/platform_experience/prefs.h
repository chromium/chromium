// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_PREFS_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

// The following prefs are read by the PEH, which may run in a separate
// process.
namespace platform_experience::prefs {

// Boolean pref for disabling PEH notifications.
inline constexpr char kDisablePEHNotificationsPrefName[] =
    "platform_experience_helper.disable_notifications";

void RegisterPrefs(PrefRegistrySimple& registry);

// Overrides prefs to reflect values from feature flags.
void SetPrefOverrides(PrefService& local_state);

}  // namespace platform_experience::prefs

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_PREFS_H_
