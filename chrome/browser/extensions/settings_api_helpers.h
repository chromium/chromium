// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_API_HELPERS_H_

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Returns which extension (if any) is overriding the homepage in a given
// |browser_context|.
const Extension* GetExtensionOverridingHomepage(
    content::BrowserContext* browser_context);

// Returns which extension (if any) is overriding the New Tab page in a given
// |browser_context|.
const Extension* GetExtensionOverridingNewTabPage(
    content::BrowserContext* browser_context);

// Returns which extension (if any) is overriding the homepage in a given
// |browser_context|.
const Extension* GetExtensionOverridingStartupPages(
    content::BrowserContext* browser_context);

// Returns which extension (if any) is overriding the search engine in a given
// |browser_context|.
const Extension* GetExtensionOverridingSearchEngine(
    content::BrowserContext* browser_context);

// Returns which extension (if any) is overriding the proxy in a given
// |browser_context|.
const Extension* GetExtensionOverridingProxy(
    content::BrowserContext* browser_context);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_API_HELPERS_H_
