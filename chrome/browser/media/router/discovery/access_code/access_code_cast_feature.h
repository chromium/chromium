// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_FEATURE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

class PrefRegistrySimple;
class Profile;

namespace base {
class TimeDelta;
}

namespace features {
BASE_DECLARE_FEATURE(kAccessCodeCastRememberDevices);
BASE_DECLARE_FEATURE(kAccessCodeCastTabSwitchingUI);
BASE_DECLARE_FEATURE(kAccessCodeCastFreezeUI);
}

namespace media_router {

#if !BUILDFLAG(IS_ANDROID)

namespace prefs {
// Pref name that allows the AccessCode/QR code scanning dialog button to be
// shown.
constexpr char kAccessCodeCastEnabled[] =
    "media_router.access_code_cast.enabled";

// Pref name for the pref that determines how long a scanned receiver remains in
// the receiver list. Duration is measured in seconds.
constexpr char kAccessCodeCastDeviceDuration[] =
    "media_router.access_code_cast.device_duration";

// Pref that keeps track of cast devices added on a user's profile. It is
// registered as a dictionary pref with each key being a
// MediaSink::Id(std::string) and value being a MediaSinkInternal object stores
// as a base::Value::Dict.
// Whenever a cast device is discovered via access code, a new entry will be
// added to this dictionary (or updated if the MediaSink::Id already exists).
constexpr char kAccessCodeCastDevices[] =
    "media_router.access_code_cast.devices";

// Pref that keeps track of when a cast device is added. It is be registered
// as a dictionary pref with each key being a MediaSink::Id and value being a
// base::Time.
constexpr char kAccessCodeCastDeviceAdditionTime[] =
    "media_router.access_code_cast.addition_time";
}  // namespace prefs

// Registers Access Code Cast related preferences with per-profile pref
// |registry|.
void RegisterAccessCodeProfilePrefs(PrefRegistrySimple* registry);

// Returns true if this user is allowed to use Access Codes to
// discover cast devices.
bool GetAccessCodeCastEnabledPref(Profile* profile);

// Returns the duration that a scanned cast device is allowed to remain
// in the cast list.
base::TimeDelta GetAccessCodeDeviceDurationPref(Profile* profile);

// Returns true if this user is allowed to use Access Codes to
// discover cast devices, and AccessCodeCastTabSwitchingUI flag is enabled.
bool IsAccessCodeCastTabSwitchingUiEnabled(Profile* profile);

// Returns true if this user is allowed to use Access Codes to
// discover cast devices, and AccessCodeCastFreezeUI flag is enabled.
bool IsAccessCodeCastFreezeUiEnabled(Profile* profile);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_FEATURE_H_
