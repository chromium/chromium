// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_
#define CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/continuous_search/page_category.h"
#include "url/gurl.h"

namespace continuous_search {

base::Optional<std::string> ExtractSearchQueryIfValidUrl(const GURL& url);

PageCategory GetSrpPageCategoryForUrl(const GURL& url);

}  // namespace continuous_search

#endif  // CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_
