// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_CHROMEOS_RESOLVE_TIME_ZONE_BY_GEOLOCATION_METHOD_SHORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_CHROMEOS_RESOLVE_TIME_ZONE_BY_GEOLOCATION_METHOD_SHORT_H_

#include <memory>

class Profile;

namespace extensions {
namespace settings_private {

class GeneratedPref;

// Time zone detection method (e.g. using general location vs. accurate
// location).
extern const char kResolveTimezoneByGeolocationMethodShort[];

// Constructor for kResolveTimezoneByGeolocationMethodShort preference.
std::unique_ptr<GeneratedPref>
CreateGeneratedResolveTimezoneByGeolocationMethodShort(Profile* profile);

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_CHROMEOS_RESOLVE_TIME_ZONE_BY_GEOLOCATION_METHOD_SHORT_H_
