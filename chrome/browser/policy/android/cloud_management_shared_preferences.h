// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ANDROID_CLOUD_MANAGEMENT_SHARED_PREFERENCES_H_
#define CHROME_BROWSER_POLICY_ANDROID_CLOUD_MANAGEMENT_SHARED_PREFERENCES_H_

#include <string>

#include "components/policy/policy_export.h"

namespace policy {
namespace android {

// Saves the device management token to Shared Preferences.
void SaveDmTokenInSharedPreferences(const std::string& dm_token);

// Delete the device management token from Shared Preferences.
void DeleteDmTokenFromSharedPreferences();

// Returns the DM token available from Shared Preferences or empty if the
// preference is not set.
std::string ReadDmTokenFromSharedPreferences();

}  // namespace android
}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ANDROID_CLOUD_MANAGEMENT_SHARED_PREFERENCES_H_
