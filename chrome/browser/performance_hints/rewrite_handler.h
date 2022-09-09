// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_HINTS_REWRITE_HANDLER_H_
#define CHROME_BROWSER_PERFORMANCE_HINTS_REWRITE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace performance_hints {

// RewriteHandler checks URLs to see if they match one of the preconfigured
// rewrite patterns. If so, returns the original (non-rewritten) URL.
//
// This is the case for many redirectors and click-tracking URLs such as
// https://www.google.com/url?url=https://actualurl.com.
class RewriteHandler {
 public:
  RewriteHandler();
  RewriteHandler(const RewriteHandler&);
  ~RewriteHandler();

  // If |url| matches one of the configured URLs, return the inner URL included
  // in the query params. If the URL is invalid or doesn't match one of the
  // configured URLs, return nullopt.
  absl::optional<GURL> HandleRewriteIfNecessary(const GURL& url) const;

  // Creates a RewriteHandler that handles URLs of the forms provided by the
  // config. If a syntax error prevents the config from being parsed, this will
  // return a RewriteHandler that matches no URLs (always returns nullopt).
  //
  // The config string is of the form "host/path?param,host/path?param,...".
  // All three values must be included for each form. Other components (port,
  // scheme, etc) must be omitted. Only one query param should be specified per
  // form, namely the param that contains the inner URL.
  //
  // An empty config ("") is valid, and indicates no URLs should be matched.
  static RewriteHandler FromConfigString(const std::string& config);

 private:
  struct UrlRule {
    // The host to match. No scheme, port, etc is included.
    std::string host;
    // The path to match. Includes the starting "/".
    std::string path;
    // The query param that contains the inner URL.
    std::string query_param;
  };

  // The URL forms that this RewriteHandler can process.
  std::vector<UrlRule> url_rules_;
};

}  // namespace performance_hints

#endif  // CHROME_BROWSER_PERFORMANCE_HINTS_REWRITE_HANDLER_H_
