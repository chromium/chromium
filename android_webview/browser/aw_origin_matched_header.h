// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_MATCHED_HEADER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_MATCHED_HEADER_H_

#include "base/memory/ref_counted.h"
#include "components/origin_matcher/origin_matcher.h"

namespace android_webview {

// Struct class to hold information about a static header name-value pair that
// should be attached to origins that match a set of matching rules.
//
// This class is RefCountedThreadSafe so it can be safely shared with the IO
// thread without the need to create copies. Objects of the class are immutable
// and do not contain any locking.
class AwOriginMatchedHeader
    : public base::RefCountedThreadSafe<AwOriginMatchedHeader> {
 public:
  AwOriginMatchedHeader(const AwOriginMatchedHeader&) = delete;
  AwOriginMatchedHeader& operator=(const AwOriginMatchedHeader&) = delete;

  AwOriginMatchedHeader(std::string name,
                        std::string value,
                        origin_matcher::OriginMatcher origin_matcher);

  std::string_view name() const { return name_; }
  std::string_view value() const { return value_; }
  bool MatchesOrigin(const url::Origin& origin) const;

 private:
  friend class base::RefCountedThreadSafe<AwOriginMatchedHeader>;

  std::string name_;
  std::string value_;
  origin_matcher::OriginMatcher matcher_;

  ~AwOriginMatchedHeader();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_MATCHED_HEADER_H_
