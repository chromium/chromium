// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_PREF_UTIL_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_PREF_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

class PrefService;

namespace ash::platform_keys::internal {

// Returns true if |public_key_spki_der| is marked for corporate usage in
// |profile_prefs|. Note: Only user keys are explicitly marked for corporate
// usage in the PrefService corresponding to the user's profile.
bool IsUserKeyMarkedCorporateInPref(
    const std::vector<uint8_t>& public_key_spki_der,
    PrefService* profile_prefs);

// Marks |public_key_spki_der| for corporate usage in |profile_prefs|.
// Note: This function will mark the key for corporate usage in |profile_prefs|
// even if the key is not accessible to that profile, so use it after making
// sure that the key is accessible to the user's profile.
void MarkUserKeyCorporateInPref(const std::vector<uint8_t>& public_key_spki_der,
                                PrefService* profile_prefs);

}  // namespace ash::platform_keys::internal

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_PREF_UTIL_H_
