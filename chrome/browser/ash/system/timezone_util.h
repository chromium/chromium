// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_UTIL_H_
#define CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"

class Profile;

namespace user_manager {
class User;
}

namespace ash {

struct TimeZoneResponseData;

namespace system {

std::optional<std::string> GetCountryCodeFromTimezoneIfAvailable(
    const std::string& timezone);

// Gets the current timezone's display name.
std::u16string GetCurrentTimezoneName();

// Creates a list of pairs of each timezone's ID and name.
base::Value::List GetTimezoneList();

// Returns true if device is managed and has SystemTimezonePolicy set.
bool HasSystemTimezonePolicy();

// Apply TimeZone update from TimeZoneProvider.
void ApplyTimeZone(const TimeZoneResponseData* timezone);

// Returns true if given timezone preference is enterprise-managed.
// Works for:
// - kSystemTimezone
// - prefs::kUserTimezone
// - prefs::kResolveTimezoneByGeolocationMethod
bool IsTimezonePrefsManaged(const std::string& pref_name);

// Updates system timezone from user profile data if needed.
// This is called from `Preferences` after updating profile
// preferences to apply new value to system time zone.
void UpdateSystemTimezone(Profile* profile);

// Returns true if the given user is allowed to set the system timezone - that
// is, the single timezone at TimezoneSettings::GetInstance()->GetTimezone(),
// which is also stored in a file at /var/lib/timezone/localtime.
bool CanSetSystemTimezone(const user_manager::User* user);

// Set system timezone to the given |timezone_id|, as long as the given |user|
// is allowed to set it (so not a guest or public account).
// Updates only the global system timezone - not specific to the user - and
// doesn't care if perUserTimezone is enabled.
// Returns |true| if the system timezone is set, false if the given user cannot.
bool SetSystemTimezone(const user_manager::User* user,
                       const std::string& timezone);

// Updates Local State preference prefs::kSigninScreenTimezone AND
// also immediately sets system timezone (ash::system::TimezoneSettings).
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
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_UTIL_H_
