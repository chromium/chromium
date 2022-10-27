// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_ORIGIN_MATCHER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_ORIGIN_MATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace url {
class Origin;
}

namespace js_injection {
class OriginMatcher;
}

namespace android_webview {

// Wrapper for a |js_incection::OriginMatcher| that allows locked updates
// to the match rules.
class AwContentsOriginMatcher
    : public base::RefCountedThreadSafe<AwContentsOriginMatcher> {
 public:
  AwContentsOriginMatcher();
  bool MatchesOrigin(const url::Origin& origin);
  // Returns the list of invalid rules.
  // If there are bad rules, no update is performed
  std::vector<std::string> UpdateRuleList(
      const std::vector<std::string>& rules);

 private:
  friend class base::RefCountedThreadSafe<AwContentsOriginMatcher>;
  ~AwContentsOriginMatcher();

  base::Lock lock_;
  std::unique_ptr<js_injection::OriginMatcher> origin_matcher_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_ORIGIN_MATCHER_H_
