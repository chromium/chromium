// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_CHROME_CONTENT_PROVIDER_URL_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_CHROME_CONTENT_PROVIDER_URL_UTIL_H_

#include "url/gurl.h"

namespace arc {

// Encodes the given URL to a chrome content provider URL, which can be used by
// ARC applications to access the original URL.
GURL EncodeToChromeContentProviderUrl(const GURL& url);

// Decodes the given chrome content provider URL to the original URL.
// Returns the empty GURL for invalid inputs.
GURL DecodeFromChromeContentProviderUrl(
    const GURL& chrome_content_provider_url);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_CHROME_CONTENT_PROVIDER_URL_UTIL_H_
