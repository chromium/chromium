// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_REQUEST_MATCHER_AW_URL_PATTERN_REQUEST_MATCHER_H_
#define ANDROID_WEBVIEW_BROWSER_REQUEST_MATCHER_AW_URL_PATTERN_REQUEST_MATCHER_H_

#include <memory>

#include "base/types/expected.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "services/network/public/cpp/resource_request.h"

namespace android_webview {

// A matcher that can be used to limit the scope of some WebView APIs to
// requests that match certain URL patterns.
class AwRequestMatcher {
 public:
  explicit AwRequestMatcher(
      std::vector<std::unique_ptr<url_pattern::SimpleUrlPatternMatcher>>
          url_patterns);
  AwRequestMatcher(const AwRequestMatcher&) = delete;
  AwRequestMatcher& operator=(const AwRequestMatcher&) = delete;
  ~AwRequestMatcher();

  // Convenience factory method that accepts URL Patterns as strings (following
  // the constructor string syntax of the URL Pattern API, see
  // https://developer.mozilla.org/en-US/docs/Web/API/URL_Pattern_API).
  // If any of the URL patterns cannot be parsed, an error string is returned.
  static base::expected<std::unique_ptr<AwRequestMatcher>, std::string> Create(
      const std::vector<std::string>& url_pattern_strings);

  // Returns true iff the request is matched by this matcher.
  bool Matches(const network::ResourceRequest& request) const;

 private:
  std::vector<std::unique_ptr<url_pattern::SimpleUrlPatternMatcher>>
      url_patterns_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_REQUEST_MATCHER_AW_URL_PATTERN_REQUEST_MATCHER_H_
