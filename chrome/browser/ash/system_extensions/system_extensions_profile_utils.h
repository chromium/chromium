// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROFILE_UTILS_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROFILE_UTILS_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"

namespace base {
class FilePath;
}

class Profile;

namespace ash {

// Returns true if System Extensions is enabled for the profile, including
// if the System Extensions feature flag is enabled.
bool IsSystemExtensionsEnabled(Profile* profile);

// Finds which Profile, if any, to use for System Extensions. Returns nullptr if
// System Extensions should not be enabled for the profile.
Profile* GetProfileForSystemExtensions(Profile* profile);

base::FilePath GetDirectoryForSystemExtension(Profile& profile,
                                              const SystemExtensionId& id);

base::FilePath GetSystemExtensionsProfileDir(Profile& profile);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROFILE_UTILS_H_
