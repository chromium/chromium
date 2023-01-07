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

// We have a few magic commands that don't cause navigations, but rather pop up
// dialogs. This function handles those cases, and returns true if so. In this
// case, normal tab navigation should be skipped.
bool HandleNonNavigationAboutURL(const GURL& url);

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
