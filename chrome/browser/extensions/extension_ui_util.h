// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UI_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UI_UTIL_H_

#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

namespace ui_util {

// Returns true if the extension should be displayed in the app launcher.
// Checks whether the extension is an ephemeral app or should be hidden due to
// policy.
bool ShouldDisplayInAppLauncher(const Extension* extension,
                                content::BrowserContext* context);

// Returns true if the extension can be displayed in the app launcher.
// Checks whether the extension should be hidden due to policy, but does not
// exclude ephemeral apps.
bool CanDisplayInAppLauncher(const Extension* extension,
                             content::BrowserContext* context);

// Returns true if the extension should be displayed in the browser NTP.
// Checks whether the extension is an ephemeral app or should be hidden due to
// policy.
bool ShouldDisplayInNewTabPage(const Extension* extension,
                               content::BrowserContext* context);

// If `url` is an extension URL, returns the name of the associated extension,
// with whitespace collapsed. Otherwise, returns empty string. `context` is used
// to get at the extension registry.
std::u16string GetEnabledExtensionNameForUrl(const GURL& url,
                                             content::BrowserContext* context);

// Returns whether `browser_context` contains any extensions that are manageable
// - i.e. visible to the user on the extensions settings page,
// chrome://extensions.
bool HasManageableExtensions(content::BrowserContext* browser_context);

}  // namespace ui_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UI_UTIL_H_
