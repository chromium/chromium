// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/javascript/web_context_fetcher_util.h"

// static
std::string WebContextFetcherUtil::ConvertJavascriptOutputToValidJson(
    std::string& json) {
  // If we receive an empty or near empty output return an empty JSON object.
  if (json.size() <= 2) {
    return "{}";
  }

  // Remove trailing and leading double quotation characters.
  std::string trimmed_json = json.substr(1, json.size() - 2);
  // Remove escape slash from before quotations.
  std::string substring_to_search = "\\\"";
  std::string replace_str = "\"";
  size_t pos = trimmed_json.find(substring_to_search);
  // Repeat till end is reached.
  while (pos != std::string::npos) {
    // Remove occurrence of the escape character before quotes.
    trimmed_json.replace(pos, substring_to_search.size(), replace_str);
    // Get the next occurrence from the current position.
    pos = trimmed_json.find(substring_to_search, pos + replace_str.size());
  }
  return trimmed_json;
}
