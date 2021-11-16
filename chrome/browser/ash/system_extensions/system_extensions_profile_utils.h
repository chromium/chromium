// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROFILE_UTILS_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROFILE_UTILS_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"

namespace base {
class FilePath;
}

class Profile;

base::FilePath GetDirectoryForSystemExtension(Profile& profile,
                                              const SystemExtensionId& id);

base::FilePath GetSystemExtensionsProfileDir(Profile& profile);

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROFILE_UTILS_H_
