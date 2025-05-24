// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_INCOGNITO_HELPERS_H_
#define CHROME_BROWSER_PROFILES_INCOGNITO_HELPERS_H_

namespace content {
class BrowserContext;
}

// Returns the original browser context even for Incognito contexts.
content::BrowserContext* GetBrowserContextRedirectedInIncognito(
    content::BrowserContext* context);

// Returns the original browser context even for Incognito contexts.
const content::BrowserContext* GetBrowserContextRedirectedInIncognito(
    const content::BrowserContext* context);

// Returns non-NULL even for Incognito contexts so that a separate
// instance of a service is created for the Incognito context.
content::BrowserContext* GetBrowserContextOwnInstanceInIncognito(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_PROFILES_INCOGNITO_HELPERS_H_
