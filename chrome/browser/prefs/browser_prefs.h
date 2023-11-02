// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
#define CHROME_BROWSER_PREFS_BROWSER_PREFS_H_

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Register all prefs that will be used via the local state PrefService.
void RegisterLocalState(PrefRegistrySimple* registry);

void RegisterScreenshotPrefs(PrefRegistrySimple* registry);

// Register all prefs that will be used via a PrefService attached to a user
// Profile using the locale of |g_browser_process|.
void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Register all prefs that will be used via a PrefService attached to a user
// Profile with the given |locale|.
void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                              const std::string& locale);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Register all prefs that will be used via a PrefService attached to the
// sign-in profile using the locale of |g_browser_process|.
void RegisterSigninProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif

// Migrate/cleanup deprecated prefs in |local_state|. Over time, long deprecated
// prefs should be removed as new ones are added, but this call should never go
// away (even if it becomes an empty call for some time) as it should remain
// *the* place to drop deprecated browser-level (Local State) prefs at.
void MigrateObsoleteLocalStatePrefs(PrefService* local_state);

// Migrate/cleanup deprecated prefs in |profile|'s pref store. Over time, long
// deprecated prefs should be removed as new ones are added, but this call
// should never go away (even if it becomes an empty call for some time) as it
// should remain *the* place to drop deprecated profile prefs at.
void MigrateObsoleteProfilePrefs(Profile* profile);

#endif  // CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
