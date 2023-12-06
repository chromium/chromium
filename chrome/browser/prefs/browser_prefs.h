// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
#define CHROME_BROWSER_PREFS_BROWSER_PREFS_H_

#include <string>
#include <string_view>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Gets the current country from the browser process. May be empty if the
// browser process cannot supply the country.
std::string GetCountry();

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
// sign-in profile using the locale of |g_browser_process|. |country| should be
// the permanent country code stored for this client in lowercase ISO 3166-1
// alpha-2. It can be used to pick country specific default values. May be
// empty May be empty in which case country specific preferences will be unable
// to be set.
void RegisterSigninProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                                std::string_view country);
#endif

// Migrate/cleanup deprecated prefs in |local_state|. Over time, long deprecated
// prefs should be removed as new ones are added, but this call should never go
// away (even if it becomes an empty call for some time) as it should remain
// *the* place to drop deprecated browser-level (Local State) prefs at.
void MigrateObsoleteLocalStatePrefs(PrefService* local_state);

// Migrate/cleanup deprecated prefs in |profile_prefs|. Over time, long
// deprecated prefs should be removed as new ones are added, but this call
// should never go away (even if it becomes an empty call for some time) as it
// should remain *the* place to drop deprecated profile prefs at.
void MigrateObsoleteProfilePrefs(PrefService* profile_prefs,
                                 const base::FilePath& profile_path);

#endif  // CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
