// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_preloading_utils.h"

#include "android_webview/common/aw_features.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "net/http/http_no_vary_search_data.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwNoVarySearchData_jni.h"
#include "android_webview/browser_jni_headers/AwPrefetchParameters_jni.h"

namespace android_webview {

net::HttpRequestHeaders GetAdditionalHeadersFromPrefetchParameters(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& prefetch_params) {
  // TODO(crbug.com/372915075): Implement tests for adding additional headers.
  net::HttpRequestHeaders additional_headers = {};
  if (prefetch_params) {
    std::map<std::string, std::string> additional_headers_map =
        Java_AwPrefetchParameters_getAdditionalHeaders(env, prefetch_params);

    for (const auto& header : additional_headers_map) {
      additional_headers.SetHeader(header.first, header.second);
    }
  }
  return additional_headers;
}

std::optional<net::HttpNoVarySearchData>
GetExpectedNoVarySearchFromPrefetchParameters(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& prefetch_params) {
  // TODO(crbug.com/372915075): Implement tests for constructing expected no
  // vary search.
  std::optional<net::HttpNoVarySearchData> expected_no_vary_search;
  if (prefetch_params) {
    base::android::ScopedJavaLocalRef<jobject> no_vary_search_jobj =
        Java_AwPrefetchParameters_getExpectedNoVarySearch(env, prefetch_params);

    if (no_vary_search_jobj) {
      const bool vary_on_key_order = static_cast<bool>(
          Java_AwNoVarySearchData_getVaryOnKeyOrder(env, no_vary_search_jobj));
      const bool ignore_differences_in_params = static_cast<bool>(
          Java_AwNoVarySearchData_getIgnoreDifferencesInParameters(
              env, no_vary_search_jobj));

      if (ignore_differences_in_params) {
        expected_no_vary_search =
            net::HttpNoVarySearchData::CreateFromVaryParams(
                Java_AwNoVarySearchData_getConsideredQueryParameters(
                    env, no_vary_search_jobj),
                vary_on_key_order);
      } else {
        expected_no_vary_search =
            net::HttpNoVarySearchData::CreateFromNoVaryParams(
                Java_AwNoVarySearchData_getIgnoredQueryParameters(
                    env, no_vary_search_jobj),
                vary_on_key_order);
      }
    }
  }
  return expected_no_vary_search;
}

bool GetIsJavaScriptEnabledFromPrefetchParameters(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& prefetch_params) {
  if (!prefetch_params) {
    return false;
  }
  return Java_AwPrefetchParameters_getIsJavascriptEnabled(env, prefetch_params);
}

// TODO(crbug.com/455296998): Remove this code for M145.
bool GetShouldBypassHttpCacheFromHeaders(net::HttpRequestHeaders& headers,
                                         bool remove_header) {
  std::optional<std::string> header_value =
      headers.GetHeader(kDisableHttpCacheHeader);
  if (header_value) {
    bool should_bypass = (header_value.value() == "1");
    if (remove_header) {
      headers.RemoveHeader(kDisableHttpCacheHeader);
    }
    return should_bypass;
  }
  return false;
}

}  // namespace android_webview

DEFINE_JNI(AwNoVarySearchData)
DEFINE_JNI(AwPrefetchParameters)
