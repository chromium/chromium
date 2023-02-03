// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PROFILE_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_PROFILE_UTIL_H_

#include "build/chromeos_buildflags.h"

class Profile;

namespace extensions::profile_util {

bool ProfileCanUseNonComponentExtensions(const Profile* profile);

}  // namespace extensions::profile_util

#endif  // CHROME_BROWSER_EXTENSIONS_PROFILE_UTIL_H_
