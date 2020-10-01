// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_PREF_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_PREF_UTIL_H_

#include <string>

class PrefService;

namespace chromeos {
namespace platform_keys {

// Note: Functions in this namespace are meant for internal use by key
// permissions classes. Please use KeyPermissionsService instead.
namespace internal {

// Returns true if |public_key_spki_der| is marked for corporate usage in
// |profile_prefs|. Note: Only user keys are explicitly marked for corporate
// usage in the PrefService corresponding to the user's profile.
bool IsUserKeyMarkedCorporateInPref(const std::string& public_key_spki_der,
                                    PrefService* profile_prefs);

// Marks |public_key_spki_der| for corporate usage in |profile_prefs|.
// Note: This function will mark the key for corporate usage in |profile_prefs|
// even if the key is not accessible to that profile, so use it after making
// sure that the key is accessible to the user's profile.
void MarkUserKeyCorporateInPref(const std::string& public_key_spki_der,
                                PrefService* profile_prefs);

}  // namespace internal
}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_PREF_UTIL_H_
