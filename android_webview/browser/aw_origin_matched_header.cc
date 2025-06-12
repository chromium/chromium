// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_origin_matched_header.h"

#include "components/origin_matcher/origin_matcher.h"

namespace android_webview {

AwOriginMatchedHeader::AwOriginMatchedHeader(
    std::string name,
    std::string value,
    origin_matcher::OriginMatcher origin_matcher)
    : name_(std::move(name)),
      value_(std::move(value)),
      matcher_(std::move(origin_matcher)) {}

AwOriginMatchedHeader::~AwOriginMatchedHeader() = default;

bool AwOriginMatchedHeader::MatchesOrigin(const url::Origin& origin) const {
  return matcher_.Matches(origin);
}

}  // namespace android_webview
