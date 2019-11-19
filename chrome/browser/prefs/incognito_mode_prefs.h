// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
#define CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"

class PrefService;
class Profile;

namespace base {
class CommandLine;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Specifies Incognito mode availability preferences.
class IncognitoModePrefs {
 public:
  // Possible values for Incognito mode availability. Please, do not change
  // the order of entries since numeric values are exposed to users.
  enum Availability {
    // Incognito mode enabled. Users may open pages in both Incognito mode and
    // normal mode (usually the default behaviour).
    ENABLED = 0,
    // Incognito mode disabled. Users may not open pages in Incognito mode.
    // Only normal mode is available for browsing.
    DISABLED,
    // Incognito mode forced. Users may open pages *ONLY* in Incognito mode.
    // Normal mode is not available for browsing.
    FORCED,

    AVAILABILITY_NUM_TYPES
  };

  static constexpr Availability kDefaultAvailability = ENABLED;

  // Register incognito related preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns kIncognitoModeAvailability preference value stored
  // in the given pref service.
  static Availability GetAvailability(const PrefService* prefs);

  // Sets kIncognitoModeAvailability preference to the specified availability
  // value.
  static void SetAvailability(PrefService* prefs,
                              const Availability availability);

  // Converts in_value into the corresponding Availability value. Returns true
  // if conversion is successful (in_value is valid). Otherwise, returns false
  // and *out_value is set to ENABLED.
  static bool IntToAvailability(int in_value, Availability* out_value);

  // Returns true if the browser should start in incognito mode.
  static bool ShouldLaunchIncognito(const base::CommandLine& command_line,
                                    const PrefService* prefs);

  // Returns true if |profile| can open a new Browser. This checks the incognito
  // availability policies and verifies if the |profile| type is allowed to
  // open new windows.
  static bool CanOpenBrowser(Profile* profile);

#if defined(OS_WIN)
  // Calculates and caches the platform parental controls enable value on a
  // worker thread.
  static void InitializePlatformParentalControls();
#endif

  // Returns whether parental controls have been enabled on the platform. This
  // method evaluates and caches if the platform controls have been enabled on
  // the first call, which must be on the UI thread when IO and blocking are
  // allowed. Subsequent calls may be from any thread.
  static bool ArePlatformParentalControlsEnabled() WARN_UNUSED_RESULT;

 private:
  // Specifies whether parental controls should be checked. See comment below.
  enum GetAvailabilityMode {
    CHECK_PARENTAL_CONTROLS,
    DONT_CHECK_PARENTAL_CONTROLS,
  };

  // Internal version of GetAvailability() that specifies whether parental
  // controls should be checked (which is expensive and not always necessary
  // to do - such as when checking for FORCED state).
  static Availability GetAvailabilityInternal(const PrefService* pref_service,
                                              GetAvailabilityMode mode);

  DISALLOW_IMPLICIT_CONSTRUCTORS(IncognitoModePrefs);
};

#endif  // CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
