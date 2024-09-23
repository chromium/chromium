// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_web_resource_intercept_response.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "components/embedder_support/android/util/web_resource_response.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwWebResourceInterceptResponse_jni.h"

using base::android::ScopedJavaLocalRef;

namespace android_webview {

AwWebResourceInterceptResponse::AwWebResourceInterceptResponse(
    const base::android::JavaRef<jobject>& obj)
    : java_object_(obj) {}

AwWebResourceInterceptResponse::~AwWebResourceInterceptResponse() = default;

bool AwWebResourceInterceptResponse::RaisedException(JNIEnv* env) const {
  return Java_AwWebResourceInterceptResponse_getRaisedException(env,
                                                                java_object_);
}

bool AwWebResourceInterceptResponse::HasResponse(JNIEnv* env) const {
  return !!Java_AwWebResourceInterceptResponse_getResponse(env, java_object_);
}

std::unique_ptr<embedder_support::WebResourceResponse>
AwWebResourceInterceptResponse::GetResponse(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> j_response =
      Java_AwWebResourceInterceptResponse_getResponse(env, java_object_);
  if (!j_response)
    return nullptr;
  return std::make_unique<embedder_support::WebResourceResponse>(j_response);
}

}  // namespace android_webview
