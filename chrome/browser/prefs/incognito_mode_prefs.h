// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
#define CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_

class PrefService;
class Profile;

namespace base {
class CommandLine;
}

namespace policy {
enum class IncognitoModeAvailability;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Specifies Incognito mode availability preferences.
class IncognitoModePrefs {
 public:
  static const policy::IncognitoModeAvailability kDefaultAvailability;

  IncognitoModePrefs() = delete;
  IncognitoModePrefs(const IncognitoModePrefs&) = delete;
  IncognitoModePrefs& operator=(const IncognitoModePrefs&) = delete;

  // Register incognito related preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns kIncognitoModeAvailability preference value stored
  // in the given pref service.
  static policy::IncognitoModeAvailability GetAvailability(
      const PrefService* prefs);

  // Sets kIncognitoModeAvailability preference to the specified availability
  // value.
  static void SetAvailability(
      PrefService* prefs,
      const policy::IncognitoModeAvailability availability);

  // Converts in_value into the corresponding Availability value. Returns true
  // if conversion is successful (in_value is valid). Otherwise, returns false
  // and *out_value is set to ENABLED.
  static bool IntToAvailability(int in_value,
                                policy::IncognitoModeAvailability* out_value);

  // Returns true if the initial browser should start in incognito mode.
  static bool ShouldLaunchIncognito(const base::CommandLine& command_line,
                                    const PrefService* prefs);

  // Returns true if subsequent browsers should be opened in incognito mode.
  static bool ShouldOpenSubsequentBrowsersInIncognito(
      const base::CommandLine& command_line,
      const PrefService* prefs);

  // Returns true if |profile| can open a new Browser. This checks the incognito
  // availability policies and verifies if the |profile| type is allowed to
  // open new windows.
  static bool CanOpenBrowser(Profile* profile);

  // Returns true if incognito mode is allowed in |profile|.
  [[nodiscard]] static bool IsIncognitoAllowed(Profile* profile);

  // Returns whether parental controls have been enabled on the platform. This
  // method evaluates and caches if the platform controls have been enabled on
  // the first call, which must be on the UI thread when IO and blocking are
  // allowed. Subsequent calls may be from any thread.
  [[nodiscard]] static bool ArePlatformParentalControlsEnabled();

 private:
  // Specifies whether parental controls should be checked. See comment below.
  enum GetAvailabilityMode {
    CHECK_PARENTAL_CONTROLS,
    DONT_CHECK_PARENTAL_CONTROLS,
  };

  // Internal version of GetAvailability() that specifies whether parental
  // controls should be checked (which is expensive and not always necessary
  // to do - such as when checking for FORCED state).
  static policy::IncognitoModeAvailability GetAvailabilityInternal(
      const PrefService* pref_service,
      GetAvailabilityMode mode);

  // Internal version of ShouldLaunchIncognito() and
  // ShouldOpenSubsequentBrowsersInIncognito() that specifies whether it is for
  // subsequent browsers or not.
  static bool ShouldLaunchIncognitoInternal(
      const base::CommandLine& command_line,
      const PrefService* prefs,
      const bool for_subsequent_browsers);
};

#endif  // CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
