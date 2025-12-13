// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_WINDOW_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_WINDOW_UTIL_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// This file and namespace contain a set of utility functions used to look up
// browser windows for various properties.
// NOTE: Most of the time, you should *NOT* be using these functions. Typically,
// different systems / classes / objects should not need to look up a browser
// window; they should instead be tied to a given browser window (e.g., managed
// by it, likely indirectly), or should operate independently of browser
// windows (e.g., be a Profile-keyed service). Looking up browser windows is
// fragile, especially with regard to activation order, and may be non-
// deterministic. See also //docs/chrome_browser_design_principles.md.
//
// The reason these exist for extensions is that there are certain circumstances
// in which extensions code *does* need to use these, for instance to look up
// a browser from an API call triggered by an extension.
namespace extensions::browser_window_util {

// Returns the BrowserWindowInterface that contains the given `tab_contents`,
// if any. If the contents does not live in a tab list, this will return
// nullptr.
BrowserWindowInterface* GetBrowserForTabContents(
    content::WebContents& tab_contents);

// Returns the last active browser with the given `profile`. If
// `include_incognito_or_parent` is true, this will also return a browser
// that crosses the incognito boundary.
BrowserWindowInterface* GetLastActiveBrowserWithProfile(
    Profile& profile,
    bool include_incognito_or_parent);

}  // namespace extensions::browser_window_util

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_WINDOW_UTIL_H_
