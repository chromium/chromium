// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FAVICON_FAVICON_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_FAVICON_FAVICON_UTIL_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/favicon_base/favicon_types.h"

class GURL;

namespace base {
class CancelableTaskTracker;
}

namespace chrome {
struct ParsedFaviconPath;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;

namespace favicon_util {

// On errors, the callback will be invoked with a null bitmap_data.
using FaviconCallback =
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> bitmap_data)>;

// Fetch favicon asynchronously.
void GetFaviconForExtensionRequest(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   const GURL& favicon_url,
                                   base::CancelableTaskTracker* tracker,
                                   FaviconCallback callback);

// Parses the given favicon_url, populating parsed with the result.
// Returns true on success. Exposed for testing purposes.
bool ParseFaviconPath(const GURL& favicon_url,
                      chrome::ParsedFaviconPath* parsed);

}  // namespace favicon_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FAVICON_FAVICON_UTIL_H_
