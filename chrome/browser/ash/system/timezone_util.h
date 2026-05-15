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

namespace ash {

struct TimeZoneResponseData;

namespace system {

std::optional<std::string> GetCountryCodeFromTimezoneIfAvailable(
    const std::string& timezone);

// Gets the current timezone's display name.
std::u16string GetCurrentTimezoneName();

// Creates a list of pairs of each timezone's ID and name.
base::ListValue GetTimezoneList();

// Returns true if device is managed and has SystemTimezonePolicy set.
bool HasSystemTimezonePolicy();

// Apply TimeZone update from TimeZoneProvider.
void ApplyTimeZone(const TimeZoneResponseData* timezone);

// Returns true if given timezone preference is enterprise-managed.
// Works for:
// - kSystemTimezone
// - ash::prefs::kUserTimezone
// - ash::prefs::kResolveTimezoneByGeolocationMethod
bool IsTimezonePrefsManaged(const std::string& pref_name);

// Updates system timezone from user profile data if needed.
// This is called from `Preferences` after updating profile
// preferences to apply new value to system time zone.
void UpdateSystemTimezone(Profile* profile);

// This is called from UI code to apply user-selected time zone.
void SetTimezoneFromUI(Profile* profile, const std::string& timezone_id);

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_UTIL_H_
