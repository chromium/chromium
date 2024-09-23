// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_UTIL_H_


namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionPrefs;

// Gets whether the bookmark app is locally installed. Defaults to true if the
// extension pref that stores this isn't set.
// Note this can be called for hosted apps which should use the default.
bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension);
bool BookmarkAppIsLocallyInstalled(const ExtensionPrefs* prefs,
                                   const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_UTIL_H_
