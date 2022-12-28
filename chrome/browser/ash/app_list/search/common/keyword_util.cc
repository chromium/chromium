// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"

namespace app_list {

std::vector<std::string> TokenizeQuery(const std::string& query) {
  // TODO(b/262623111): Implement function to tokenize user query into
  // individual tokens.
  return std::vector<std::string>();
}

std::vector<std::string> ExtractKeyword(
    const std::vector<std::string>& query_tokens) {
  // TODO(b/262623111): Implement function to identify and extract the keywords
  // from list of tokens.

  return std::vector<std::string>();
}

}  // namespace app_list
