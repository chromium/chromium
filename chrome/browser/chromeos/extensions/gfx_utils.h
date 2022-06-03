// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "chrome/browser/extensions/chrome_app_icon.h"

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
}

namespace extensions {
namespace util {

// Returns true if the equivalent Play Store app, which is corresponding to the
// Chrome extension (identified by |extension_id|), has been installed.
bool HasEquivalentInstalledArcApp(content::BrowserContext* context,
                                  const std::string& extension_id);

// Returns true if the equivalent Play Store app, which is corresponding to the
// Chrome extension (identified by |extension_id|), has been installed.
// |arc_apps| receives ids of Play Store apps.
bool GetEquivalentInstalledArcApps(content::BrowserContext* context,
                                   const std::string& extension_id,
                                   std::unordered_set<std::string>* arc_apps);

// Returns the equivalent app IDs that have been installed to the
// Playstore app (identified by |arc_package_name|).
const std::vector<std::string> GetEquivalentInstalledAppIds(
    const std::string& arc_package_name);

// Returns the equivalent Chrome extensions that have been installed to the
// Playstore app (identified by |arc_package_name|). Returns an empty vector if
// there is no such Chrome extension.
const std::vector<std::string> GetEquivalentInstalledExtensions(
    content::BrowserContext* context,
    const std::string& arc_package_name);

// Returns whether to call ApplyChromeBadge.
bool ShouldApplyChromeBadge(content::BrowserContext* context,
                            const std::string& extension_id);

// Returns whether to call ApplyChromeBadge.
bool ShouldApplyChromeBadgeToWebApp(content::BrowserContext* context,
                                    const std::string& web_app_id);

// Applies an additional badge identified by |badge_type|.
void ApplyBadge(gfx::ImageSkia* icon_out, ChromeAppIcon::Badge badge_type);

}  // namespace util
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_
