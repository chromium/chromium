// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_origin_matched_header.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "components/origin_matcher/origin_matcher.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwOriginMatchedHeader_jni.h"

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

scoped_refptr<AwOriginMatchedHeader> AwOriginMatchedHeader::MergedWithMatcher(
    origin_matcher::OriginMatcher&& other) const {
  std::vector<std::string> new_rules = other.Serialize();
  // Use a set to avoid linear lookups in `new_rules`.
  base::flat_set<std::string_view> new_rules_set(new_rules.begin(),
                                                 new_rules.end());

  // Merge the existing rules into the new matcher without duplicates.
  std::vector<std::string> existing_rules = matcher_.Serialize();
  for (const std::string& existing_rule : existing_rules) {
    if (!new_rules_set.contains(existing_rule)) {
      other.AddRuleFromString(existing_rule);
    }
  }

  return base::MakeRefCounted<AwOriginMatchedHeader>(name_, value_,
                                                     std::move(other));
}

// static
std::vector<std::pair<std::string_view, std::string>>
AwOriginMatchedHeader::GetCombinedMatchingHeaders(
    base::span<scoped_refptr<AwOriginMatchedHeader>> headers,
    const url::Origin& origin) {
  // Comparator for case-insensitive header name comparison.
  // This is used to coalesce headers with different casing into a single
  // unified header.
  struct HeaderNameComparator {
   public:
    int operator()(const std::string_view a, const std::string_view b) const {
      return base::CompareCaseInsensitiveASCII(a, b);
    }
  };
  base::flat_map<std::string_view, std::string, HeaderNameComparator>
      combined_headers;

  for (const auto& header : headers) {
    if (!header->MatchesOrigin(origin)) {
      continue;
    }

    auto combined_itr = combined_headers.find(header->name());
    if (combined_itr == combined_headers.end()) {
      combined_headers.emplace(header->name(), header->value());
    } else {
      combined_itr->second.append(",");
      combined_itr->second.append(header->value());
    }
  }
  // Returning the list of pairs allows structured binding at the call site.
  return std::move(combined_headers).extract();
}

bool AwOriginMatchedHeader::MatchesNameValue(
    const std::string& target_name,
    const std::optional<std::string>& target_value) const {
  if (!base::EqualsCaseInsensitiveASCII(name_, target_name)) {
    return false;
  }
  if (!target_value) {
    return true;
  }
  return value_ == *target_value;
}

jni_zero::ScopedJavaLocalRef<jobject> AwOriginMatchedHeader::ToJavaObject(
    JNIEnv* env) {
  return Java_AwOriginMatchedHeader_create(env, name_, value_,
                                           matcher_.Serialize());
}

}  // namespace android_webview

DEFINE_JNI(AwOriginMatchedHeader)
