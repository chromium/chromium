// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_DEVICE_ID_HELPER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_DEVICE_ID_HELPER_H_

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class Profile;

// Returns the device ID that is scoped to single signin.
// All refresh tokens for |profile| are annotated with this device ID when they
// are requested.
// On non-ChromeOS platforms, this is equivalent to:
//     signin::GetSigninScopedDeviceId(profile->GetPrefs());
std::string GetSigninScopedDeviceIdForProfile(Profile* profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Helper method. The device ID should generally be obtained through
// GetSigninScopedDeviceIdForProfile().
// If `for_ephemeral` is true, special kind of device ID for ephemeral users is
// generated.
// If `for_ephemeral` is false, this function will cache (in-memory) its return
// value and keep returning it - if `kStableDeviceId` feature is enabled.
std::string GenerateSigninScopedDeviceId(bool for_ephemeral);

// Moves any existing device ID out of the pref service into the UserManager,
// and creates a new ID if it is empty.
void MigrateSigninScopedDeviceId(Profile* profile);

#endif

#endif  // CHROME_BROWSER_SIGNIN_CHROME_DEVICE_ID_HELPER_H_
