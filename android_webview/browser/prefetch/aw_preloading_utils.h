// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PRELOADING_UTILS_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PRELOADING_UTILS_H_

#include <jni.h>

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "net/http/http_request_headers.h"

namespace net {
class HttpNoVarySearchData;
}  // namespace net

namespace android_webview {

net::HttpRequestHeaders GetAdditionalHeadersFromPrefetchParameters(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& prefetch_params);

std::optional<net::HttpNoVarySearchData>
GetExpectedNoVarySearchFromPrefetchParameters(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& prefetch_params);

bool GetIsJavaScriptEnabledFromPrefetchParameters(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& prefetch_params);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PRELOADING_UTILS_H_
