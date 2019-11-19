// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
#define CHROME_BROWSER_PREFS_BROWSER_PREFS_H_

#include <set>

#include "build/build_config.h"
#include "components/prefs/pref_value_store.h"

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

#if defined(OS_CHROMEOS)
// Register all prefs that will be used via a PrefService attached to the
// sign-in profile using the locale of |g_browser_process|.
void RegisterSigninProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif

// Migrate/cleanup deprecated prefs in |local_state|. Over time, long deprecated
// prefs should be removed as new ones are added, but this call should never go
// away (even if it becomes an empty call for some time) as it should remain
// *the* place to drop deprecated browser prefs at.
void MigrateObsoleteBrowserPrefs(Profile* profile, PrefService* local_state);

// Migrate/cleanup deprecated prefs in |profile|'s pref store. Over time, long
// deprecated prefs should be removed as new ones are added, but this call
// should never go away (even if it becomes an empty call for some time) as it
// should remain *the* place to drop deprecated profile prefs at.
void MigrateObsoleteProfilePrefs(Profile* profile);

#endif  // CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
