// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

using KeywordToProvidersMap =
    base::flat_map<std::u16string, std::vector<ProviderType>>;

using KeywordToProvidersPairs =
    std::vector<std::pair<std::u16string, std::vector<ProviderType>>>;

// Provided the list of tokens produced from the user query, returns a list of
// keywords and its associated SearchProviders.
//   - A given keyword can be associated with 1 or more SearchProviders.
//   - Multiple keywords may map to the same SearchProvider.
KeywordToProvidersPairs ExtractKeyword(const std::u16string& query);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_
