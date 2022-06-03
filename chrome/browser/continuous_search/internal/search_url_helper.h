// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_
#define CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_

#include <string>

#include "chrome/browser/continuous_search/page_category.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace continuous_search {

absl::optional<std::string> ExtractSearchQueryIfValidUrl(const GURL& url);

PageCategory GetSrpPageCategoryForUrl(const GURL& url);

GURL GetOriginalUrlFromWebContents(content::WebContents* web_contents);

}  // namespace continuous_search

#endif  // CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_
