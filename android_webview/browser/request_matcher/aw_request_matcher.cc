// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/request_matcher/aw_request_matcher.h"

#include "base/strings/stringprintf.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"

namespace android_webview {

AwRequestMatcher::AwRequestMatcher(
    std::vector<std::unique_ptr<url_pattern::SimpleUrlPatternMatcher>>
        url_patterns)
    : url_patterns_(std::move(url_patterns)) {}

AwRequestMatcher::~AwRequestMatcher() = default;

// static
base::expected<std::unique_ptr<AwRequestMatcher>, std::string>
AwRequestMatcher::Create(const std::vector<std::string>& url_pattern_strings) {
  if (url_pattern_strings.empty()) {
    return base::unexpected("Must provide at least one URL pattern string.");
  }

  std::vector<std::unique_ptr<url_pattern::SimpleUrlPatternMatcher>>
      url_patterns;
  url_patterns.reserve(url_pattern_strings.size());

  for (size_t i = 0; i < url_pattern_strings.size(); i++) {
    auto url_pattern = url_pattern::SimpleUrlPatternMatcher::Create(
        url_pattern_strings[i], /* base_url= */ nullptr);
    if (!url_pattern.has_value()) {
      return base::unexpected(base::StringPrintf(
          "Failed to parse URL pattern at index %d (\"%s\"): %s", i,
          url_pattern_strings[i], url_pattern.error()));
    }
    url_patterns.push_back(std::move(url_pattern.value()));
  }
  return std::make_unique<AwRequestMatcher>(std::move(url_patterns));
}

bool AwRequestMatcher::Matches(const network::ResourceRequest& request) const {
  for (const auto& url_pattern : url_patterns_) {
    if (url_pattern->Match(request.url)) {
      return true;
    }
  }
  return false;
}

}  // namespace android_webview
