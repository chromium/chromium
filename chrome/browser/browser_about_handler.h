// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#define CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_

class GURL;

namespace content {
class BrowserContext;
}

// Rewrites chrome://about -> chrome://chrome-urls and chrome://sync ->
// chrome://sync-internals.  Used with content::BrowserURLHandler.
bool HandleChromeAboutAndChromeSyncRewrite(
    GURL* url,
    content::BrowserContext* browser_context);

// Handles special about: URLs that trigger actions instead of navigation.
// If |context|, URL blocking policies are checked first; if blocked, the
// action is prevented. Returns true if handled (action occurred or was
// policy-blocked), so normal navigation is skipped.
bool HandleNonNavigationAboutURL(const GURL& url,
                                 content::BrowserContext* context);

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
