// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
#define CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace base {
class DictionaryValue;
class FilePath;
class Time;
}

class GURL;
struct WebApplicationInfo;

namespace extensions {
class Extension;

// Creates a DictionaryValue with a single URL handler for |scope_url| and
// |title|. |title| is meant to appear in relevant UI surfaces but it's not used
// anywhere yet. The resulting DictionaryValue can be used as the "url_handlers"
// field in a Chrome Apps manifest.
//
// To create a URL handler that will match the same URLs as the "within
// scope" algorithm of the Web Manifest spec, we remove everything
// but the origin and path and append a wildcard, i.e. "*", to the result.
// According to the Web Manifest spec, a URL |url| is within scope of
// |scope_url| if |url|'s origin is the same as |scope_url|'s origin and
// |url|'s path starts with |scope_url|'s path.
// Note that this results in some unexpected URLs being within scope
// according to the spec:
// Suppose |scope_url| is "https://example.com/foo" and |url| is
// "https://example.com/foobar.html", then according to the spec algorithm
// |url| is within scope.
// See https://github.com/w3c/manifest/issues/554 for details.
//
// GetScopeURLFromBookmarkApp() reverses this operation, i.e. removes
// the '*' from the scope URL handler, to retrieve the scope for a Bookmark App.
// So if you change this, you also have to change GetScopeURLFromBookmarkApp().
std::unique_ptr<base::DictionaryValue> CreateURLHandlersForBookmarkApp(
    const GURL& scope_url,
    const std::u16string& title);

// Retrieves the scope URL from a Bookmark App's URL handlers.
GURL GetScopeURLFromBookmarkApp(const Extension* extension);

// Generates a version number for an extension from a time. The goal is to make
// use of the version number to communicate the date in a human readable form,
// while maintaining high enough resolution to change each time an app is
// reinstalled. The version that is returned has the format:
//
// <year>.<month>.<day>.<fraction>
//
// fraction is represented as a number between 0 and 2^16-1. Each unit is
// ~1.32 seconds.
std::string ConvertTimeToExtensionVersion(const base::Time& time);

// Wraps the specified web app in an extension. The extension is created
// unpacked in the system temp dir. Returns a valid extension that the caller
// should take ownership on success, or NULL and |error| on failure.
//
// NOTE: The app created is always marked as a bookmark app.
// NOTE: This function does file IO and should not be called on the UI thread.
// NOTE: The caller takes ownership of the directory at extension->path() on the
// returned object.
scoped_refptr<Extension> ConvertWebAppToExtension(
    const WebApplicationInfo& web_app_info,
    const base::Time& create_time,
    const base::FilePath& extensions_dir,
    int extra_creation_flags,
    mojom::ManifestLocation install_source);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
