// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_

#include <memory>

#include "base/strings/string16.h"

class Profile;

namespace base {
class ListValue;
}

namespace user_manager {
class User;
}

namespace chromeos {

struct TimeZoneResponseData;

namespace system {

// Gets the current timezone's display name.
base::string16 GetCurrentTimezoneName();

// Creates a list of pairs of each timezone's ID and name.
std::unique_ptr<base::ListValue> GetTimezoneList();

// Returns true if device is managed and has SystemTimezonePolicy set.
bool HasSystemTimezonePolicy();

// Apply TimeZone update from TimeZoneProvider.
void ApplyTimeZone(const TimeZoneResponseData* timezone);

// Returns true if given timezone preference is enterprise-managed.
// Works for:
// - prefs::kUserTimezone
// - prefs::kResolveTimezoneByGeolocationMethod
bool IsTimezonePrefsManaged(const std::string& pref_name);

// Updates system timezone from user profile data if needed.
// This is called from chromeos::Preferences after updating profile
// preferences to apply new value to system time zone.
void UpdateSystemTimezone(Profile* profile);

// Set system timezone to the given |timezone_id|, as long as the given |user|
// is allowed to set it (so not a guest or public account).
// Updates only the global system timezone - not specific to the user - and
// doesn't care if perUserTimezone is enabled.
// Returns |true| if the system timezone is set, false if the given user cannot.
bool SetSystemTimezone(const user_manager::User* user,
                       const std::string& timezone);

// Updates Local State preference prefs::kSigninScreenTimezone AND
// also immediately sets system timezone (chromeos::system::TimezoneSettings).
// This is called when there is no user session (i.e. OOBE and signin screen),
// or when device policies are updated.
void SetSystemAndSigninScreenTimezone(const std::string& timezone);

// Returns true if per-user timezone preferences are enabled.
bool PerUserTimezoneEnabled();

// This is called from UI code to apply user-selected time zone.
void SetTimezoneFromUI(Profile* profile, const std::string& timezone_id);

// Returns true if fine-grained time zone detection is enabled.
bool FineGrainedTimeZoneDetectionEnabled();

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_
