// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_
#define CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_

#include "base/optional.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace continuous_search {

base::Optional<std::string> ExtractSearchQueryIfGoogle(const GURL& url);

}  // namespace continuous_search

#endif  // CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_URL_HELPER_H_
