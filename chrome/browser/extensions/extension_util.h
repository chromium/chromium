// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "extensions/common/constants.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
}

class GURL;

namespace extensions {

class Extension;

namespace util {

// Returns true if the site URL corresponds to an extension or app which
// has isolated storage. This can be either because it is an app that
// requested this in its manifest, or because it is a policy-installed app or
// extension running on the Chrome OS sign-in profile.
bool SiteHasIsolatedStorage(const GURL& extension_site_url,
                            content::BrowserContext* context);

// Sets whether |extension_id| can run in an incognito window. Reloads the
// extension if it's enabled since this permission is applied at loading time
// only. Note that an ExtensionService must exist.
void SetIsIncognitoEnabled(const std::string& extension_id,
                           content::BrowserContext* context,
                           bool enabled);

// Returns true if |extension| can be loaded in incognito.
bool CanLoadInIncognito(const extensions::Extension* extension,
                        content::BrowserContext* context);

// Returns true if this extension can inject scripts into pages with file URLs.
bool AllowFileAccess(const std::string& extension_id,
                     content::BrowserContext* context);

// Sets whether |extension_id| can inject scripts into pages with file URLs.
// Reloads the extension if it's enabled since this permission is applied at
// loading time only. Note than an ExtensionService must exist.
void SetAllowFileAccess(const std::string& extension_id,
                        content::BrowserContext* context,
                        bool allow);

// Returns true if |extension_id| can be launched (possibly only after being
// enabled).
bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context);

// Returns true if |extension_id| can be launched without being enabled first.
bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context);

// Returns true if |extension| should be synced.
bool ShouldSync(const Extension* extension, content::BrowserContext* context);

// Returns true if |extension_id| is idle and it is safe to perform actions such
// as updating.
bool IsExtensionIdle(const std::string& extension_id,
                     content::BrowserContext* context);

// Sets the name, id, and icon resource path of the given extension into the
// returned dictionary.
std::unique_ptr<base::DictionaryValue> GetExtensionInfo(
    const Extension* extension);

// Returns the default extension/app icon (for extensions or apps that don't
// have one).
const gfx::ImageSkia& GetDefaultExtensionIcon();
const gfx::ImageSkia& GetDefaultAppIcon();

// Finds the first PWA with |url| in its scope, returns nullptr if there are
// none.
const Extension* GetInstalledPwaForUrl(
    content::BrowserContext* context,
    const GURL& url,
    base::Optional<LaunchContainer> launch_container_filter = base::nullopt);

}  // namespace util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
