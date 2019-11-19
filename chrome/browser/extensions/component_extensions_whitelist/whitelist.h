// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_EXTENSIONS_WHITELIST_WHITELIST_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_EXTENSIONS_WHITELIST_WHITELIST_H_

#include <string>

namespace extensions {

// =============================================================================
//
// ADDING NEW EXTENSIONS REQUIRES APPROVAL from Extensions Tech Lead:
// rdevlin.cronin@chromium.org
//
// The main acceptable use of extensions in the default Chrome experience (i.e.
// not installed explicitly by the user) are to implement things like the
// history or settings pages. These are things that look like web pages, load
// in response to explicit user action, and use no resources when not opened.
//
// If you are asking for approval to add a new built-in extension to Chrome
// (whether downloaded with the binary or downloaded later on-demand), check:
//
//  - It must not do anything on startup. Loading extensions processes on
//    startup can significantly slow things down.
//
//  - It must not have a background page. Extension processes use a nontrivial
//    amount of memory that makes them inappropriate for built-in features.
//
//  - Avoid event pages. Some events such as navigation will be even worse than
//    background pages since it will cause the extension to be frequently
//    loaded and unloaded. Even if your event page is "good" now, it's
//    something that is easy to regress with inocuous looking changes, so
//    try to use explicit C++ invocation of the extension when reasonable.
//
// =============================================================================

// Checks using an extension ID.
bool IsComponentExtensionWhitelisted(const std::string& extension_id);

// Checks using resource ID of manifest.
bool IsComponentExtensionWhitelisted(int manifest_resource_id);

#if defined(OS_CHROMEOS)
// Checks using extension id for sign in profile.
bool IsComponentExtensionWhitelistedForSignInProfile(
    const std::string& extension_id);
#endif

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_EXTENSIONS_WHITELIST_WHITELIST_H_
