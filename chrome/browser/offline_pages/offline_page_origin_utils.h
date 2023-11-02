// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_ORIGIN_UTILS_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_ORIGIN_UTILS_H_

#include <string>

namespace content {
class WebContents;
}

namespace offline_pages {

// Utility class for retrieving origin for a download request - whether it
// came on behalf of some app via CCT or Chrome. Methods are platform-specific.
class OfflinePageOriginUtils {
 public:
  // Retrieves the encoded origin from the |web_contents|.
  static std::string GetEncodedOriginAppFor(content::WebContents* web_contents);
};
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_ORIGIN_UTILS_H_
