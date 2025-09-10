// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_MATCHED_HEADER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_MATCHED_HEADER_H_

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/origin_matcher/origin_matcher.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/origin.h"

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

  std::tuple<const std::string&, const std::string&> as_pair() const {
    return std::tie(name_, value_);
  }

  // Utility function to help filter headers with standard library algorithms.
  // If `value` is std::nullopt, this function only matches on name.
  bool MatchesNameValue(const std::string& name,
                        const std::optional<std::string>& value) const;

  bool MatchesOrigin(const url::Origin& origin) const;

  // Returns a new AwOriginMatchedHeader which has the combined ruleset of
  // `this` and the `other` OriginMatcher.
  scoped_refptr<AwOriginMatchedHeader> MergedWithMatcher(
      origin_matcher::OriginMatcher&& other) const;

  jni_zero::ScopedJavaLocalRef<jobject> ToJavaObject(JNIEnv* env);

  // Provide a map of header name-value pairs that match the given `origin`.
  // This function returns the header values as std::string because it may
  // combine multiple values into a single header.
  // Header values will be separated with a comma character.
  static base::flat_map<std::string_view, std::string>
  GetCombinedMatchingHeaders(
      base::span<scoped_refptr<AwOriginMatchedHeader>> headers,
      const url::Origin& origin);

 private:
  friend class base::RefCountedThreadSafe<AwOriginMatchedHeader>;

  std::string name_;
  std::string value_;
  origin_matcher::OriginMatcher matcher_;

  ~AwOriginMatchedHeader();
};

}  // namespace android_webview

namespace jni_zero {
template <>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const scoped_refptr<android_webview::AwOriginMatchedHeader>& header) {
  return header->ToJavaObject(env);
}
}  // namespace jni_zero

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_MATCHED_HEADER_H_
