// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GUEST_PROFILE_CREATION_LOGGER_H_
#define CHROME_BROWSER_PROFILES_GUEST_PROFILE_CREATION_LOGGER_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error Guest profiles not supported, this file should not be included.
#endif

class Profile;

namespace profile {

// Records the creation of the provided Guest `profile`.
void RecordGuestParentCreation(Profile* profile);

// Records the creation of the provided off-the-record Guest `profile`. Does
// nothing if there is already a child (derived OTR profile) recorded for its
// original profile.
void MaybeRecordGuestChildCreation(Profile* profile);

}  // namespace profile

#endif  // CHROME_BROWSER_PROFILES_GUEST_PROFILE_CREATION_LOGGER_H_
