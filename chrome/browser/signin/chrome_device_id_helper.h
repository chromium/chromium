// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_DEVICE_ID_HELPER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_DEVICE_ID_HELPER_H_

#include <string>

#include "build/build_config.h"

class Profile;

// Returns the device ID that is scoped to single signin.
// All refresh tokens for |profile| are annotated with this device ID when they
// are requested.
// On non-ChromeOS platforms, this is equivalent to:
//     signin::GetSigninScopedDeviceId(profile->GetPrefs());
std::string GetSigninScopedDeviceIdForProfile(Profile* profile);

#if BUILDFLAG(IS_CHROMEOS)

// Moves any existing device ID out of the pref service into the UserManager,
// and creates a new ID if it is empty.
void MigrateSigninScopedDeviceId(Profile* profile);

#endif

#endif  // CHROME_BROWSER_SIGNIN_CHROME_DEVICE_ID_HELPER_H_
