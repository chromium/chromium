// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_

#include <string>

#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/common/constants.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionPrefs;

// Gets the launch type preference. If no preference is set, returns
// LAUNCH_TYPE_DEFAULT.
// Returns LAUNCH_TYPE_WINDOW if there's no preference and
// bookmark apps are enabled.
LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                         const Extension* extension);

// Returns the LaunchType that is set in the prefs. Returns LAUNCH_TYPE_INVALID
// if no value is set in prefs.
LaunchType GetLaunchTypePrefValue(const ExtensionPrefs* prefs,
                                  const std::string& extension_id);

// Sets an extension's launch type preference and syncs the value if necessary.
void SetLaunchType(content::BrowserContext* context,
                   const std::string& extension_id,
                   LaunchType launch_type);

// Finds the right launch container based on the launch type.
// If |extension|'s prefs do not have a launch type set, then the default
// value from GetLaunchType() is used to choose the launch container.
apps::LaunchContainer GetLaunchContainer(const ExtensionPrefs* prefs,
                                         const Extension* extension);

// Returns true if a launch container preference has been specified for
// |extension|. GetLaunchContainer() will still return a default value even if
// this returns false.
bool HasPreferredLaunchContainer(const ExtensionPrefs* prefs,
                                 const Extension* extension);

// Whether |extension| will launch in a window.
bool LaunchesInWindow(content::BrowserContext* context,
                      const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LAUNCH_UTIL_H_
