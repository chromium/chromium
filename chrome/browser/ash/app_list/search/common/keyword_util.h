// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_

#include <string>
#include <vector>

namespace app_list {

// Given a user query, processes the query into tokens separated by ' '.
std::vector<std::string> TokenizeQuery(const std::string& query);

// Provided the list of tokens produced from the user query, returns
// a list of keywords and its associated SearchProvider.
std::vector<std::string> ExtractKeyword(
    const std::vector<std::string>& query_tokens);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_
