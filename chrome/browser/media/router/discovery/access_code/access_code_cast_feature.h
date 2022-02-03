// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_FEATURE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

class PrefRegistrySimple;
class PrefService;

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
}  // namespace prefs

// Registers Access Code Cast related preferences with per-profile pref
// |registry|.
void RegisterAccessCodeProfilePrefs(PrefRegistrySimple* registry);

// Returns true if this user is allowed to use Access Codes & QR codes to
// discover cast devices.
bool GetAccessCodeCastEnabledPref(PrefService* pref_service);

// Returns the duration that a scanned cast device is allowed to remain
// in the cast list.
base::TimeDelta GetAccessCodeDeviceDurationPref(PrefService* pref_service);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_FEATURE_H_
