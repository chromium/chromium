// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_origin_matcher.h"

#include "components/js_injection/common/origin_matcher.h"
#include "url/origin.h"

namespace android_webview {

AwContentsOriginMatcher::AwContentsOriginMatcher()
    : origin_matcher_(std::make_unique<js_injection::OriginMatcher>()) {}

AwContentsOriginMatcher::~AwContentsOriginMatcher() = default;

bool AwContentsOriginMatcher::MatchesOrigin(const url::Origin& origin) {
  base::AutoLock auto_lock(lock_);
  return origin_matcher_->Matches(origin);
}

std::vector<std::string> AwContentsOriginMatcher::UpdateRuleList(
    const std::vector<std::string>& rules) {
  std::vector<std::string> bad_rules;
  std::unique_ptr<js_injection::OriginMatcher> new_matcher =
      std::make_unique<js_injection::OriginMatcher>();
  for (const std::string& rule : rules) {
    if (!new_matcher->AddRuleFromString(rule))
      bad_rules.push_back(rule);
  }

  if (!bad_rules.empty())
    return bad_rules;

  {
    // Swap the pointer while locked, then release the lock before running the
    // destructor on the old (swapped-out) pointer.
    base::AutoLock auto_lock(lock_);
    origin_matcher_.swap(new_matcher);
  }
  return bad_rules;
}

}  // namespace android_webview
