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

// Specifies Incognito mode preferences.
//
// This class encapsulates the availability logic for both standard Incognito
// mode and Enterprise Isolated Mode (which can replace Incognito).
// Most callers should not care whether Incognito or Isolated Mode is active and
// should simply query general availability via `GetAvailability` or
// `IsIncognitoAllowed`. Callers that specifically need to distinguish between
// the two can query `GetType` or use `IsIncognitoTypeAllowed`.
class IncognitoModePrefs {
 public:
  // Specifies the effective type of the available Incognito mode.
  enum class IncognitoModeType {
    // Incognito browsing is disabled.
    kNone,
    // Standard Incognito mode.
    kStandard,
    // Enterprise Isolated Mode. This mode replaces standard Incognito mode when
    // enabled by the enterprise policy `IsolatedModeSettings`.
    kEnterprise,

    kNumTypes,
  };

  static const policy::IncognitoModeAvailability kDefaultAvailability;

  IncognitoModePrefs() = delete;
  IncognitoModePrefs(const IncognitoModePrefs&) = delete;
  IncognitoModePrefs& operator=(const IncognitoModePrefs&) = delete;

  // Register incognito related preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the effective Incognito mode availability for `profile`.
  //
  // It does not matter if Incognito is replaced by Enterprise Isolated Mode.
  // This method encapsulates the availability across both modes. When Isolated
  // Mode replaces Incognito, it overrides the incognito specific preference.
  static policy::IncognitoModeAvailability GetAvailability(
      const Profile* profile);

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
                                    const Profile* profile);

  // Returns true if subsequent browsers should be opened in incognito mode.
  static bool ShouldOpenSubsequentBrowsersInIncognito(
      const base::CommandLine& command_line,
      const Profile* profile);

  // Returns true if |profile| can open a new Browser. This checks the incognito
  // availability policies and verifies if the |profile| type is allowed to
  // open new windows.
  static bool CanOpenBrowser(Profile* profile);

  // Returns true if Incognito or Isolated Mode is allowed in |profile|.
  [[nodiscard]] static bool IsIncognitoAllowed(Profile* profile);

  // Returns true if the profile's Incognito mode type matches the specified
  // `type`. Note that callers which only care whether Incognito or Isolated
  // Mode is allowed should use `IsIncognitoAllowed(Profile*)` instead.
  [[nodiscard]] static bool IsIncognitoTypeAllowed(Profile* profile,
                                                   IncognitoModeType type);

  // Returns whether parental controls have been enabled on the platform. This
  // method evaluates and caches if the platform controls have been enabled on
  // the first call, which must be on the UI thread when IO and blocking are
  // allowed. Subsequent calls may be from any thread.
  [[nodiscard]] static bool ArePlatformParentalControlsEnabled();

  // Returns the Incognito mode type for the given `profile`, taking into
  // account both the Incognito mode availability policy and the Enterprise
  // Isolated Mode policy.
  //
  // Most callers do not need to distinguish between the two types and should
  // use `IsIncognitoAllowed()` to check whether `profile` can open either.
  [[nodiscard]] static IncognitoModeType GetIncognitoModeType(
      const Profile* profile);

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
      const Profile* profile,
      GetAvailabilityMode mode);

  // Internal version of ShouldLaunchIncognito() and
  // ShouldOpenSubsequentBrowsersInIncognito() that specifies whether it is for
  // subsequent browsers or not.
  static bool ShouldLaunchIncognitoInternal(
      const base::CommandLine& command_line,
      const Profile* profile,
      const bool for_subsequent_browsers);
};

#endif  // CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
