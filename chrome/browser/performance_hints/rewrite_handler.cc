// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/rewrite_handler.h"

#include <utility>

#include "base/strings/string_split.h"
#include "net/base/escape.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace performance_hints {

RewriteHandler::RewriteHandler() = default;
RewriteHandler::RewriteHandler(const RewriteHandler&) = default;
RewriteHandler::~RewriteHandler() = default;

absl::optional<GURL> RewriteHandler::HandleRewriteIfNecessary(
    const GURL& url) const {
  if (!url.is_valid()) {
    return absl::nullopt;
  }

  base::StringPiece host = url.host_piece();
  base::StringPiece path = url.path_piece();

  for (const UrlRule& url_rule : url_rules_) {
    if (host == url_rule.host && path == url_rule.path) {
      std::string query_str = url.query();
      url::Component query(0, query_str.length());
      url::Component key, value;
      while (
          url::ExtractQueryKeyValue(query_str.c_str(), &query, &key, &value)) {
        if (query_str.substr(key.begin, key.len) == url_rule.query_param) {
          // Unescape the inner URL since it was escaped to be made a query
          // param.
          std::string unescaped = net::UnescapeURLComponent(
              query_str.substr(value.begin, value.len),
              net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
          return GURL(unescaped);
        }
      }
      return absl::nullopt;
    }
  }
  return absl::nullopt;
}

RewriteHandler RewriteHandler::FromConfigString(const std::string& config) {
  RewriteHandler handler;

  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(config, '?', ',', &pairs)) {
    // Empty, will match no URLs.
    return handler;
  }

  for (const std::pair<std::string, std::string>& pair : pairs) {
    if (pair.first.empty() || pair.second.empty()) {
      continue;
    }

    RewriteHandler::UrlRule url_rule;

    const std::string& host_path = pair.first;
    size_t path_start = host_path.find('/');
    if (path_start == std::string::npos) {
      // A path must be specified, even if that path is the root ("/").
      continue;
    }

    url_rule.host = host_path.substr(0, path_start);
    url_rule.path = host_path.substr(path_start);
    url_rule.query_param = pair.second;
    handler.url_rules_.push_back(url_rule);
  }

  return handler;
}

}  // namespace performance_hints
