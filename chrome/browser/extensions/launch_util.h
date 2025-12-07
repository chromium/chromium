// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_

#include <string>

#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionPrefs;

// Sets an extension's launch type preference and syncs the value if necessary.
void SetLaunchType(content::BrowserContext* context,
                   const std::string& extension_id,
                   LaunchType launch_type);

// Finds the right launch container based on the launch type.
// If `extension`'s prefs do not have a launch type set, then the default
// value from GetLaunchType() is used to choose the launch container.
apps::LaunchContainer GetLaunchContainer(const ExtensionPrefs* prefs,
                                         const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_
