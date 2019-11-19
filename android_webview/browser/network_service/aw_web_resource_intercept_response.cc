// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_web_resource_intercept_response.h"

#include <memory>
#include <utility>

#include "android_webview/browser/network_service/aw_web_resource_response.h"
#include "android_webview/browser_jni_headers/AwWebResourceInterceptResponse_jni.h"
#include "base/android/jni_android.h"

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
  return !Java_AwWebResourceInterceptResponse_getResponse(env, java_object_)
              .is_null();
}

std::unique_ptr<AwWebResourceResponse>
AwWebResourceInterceptResponse::GetResponse(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> j_response =
      Java_AwWebResourceInterceptResponse_getResponse(env, java_object_);
  if (j_response.is_null())
    return nullptr;
  return std::make_unique<AwWebResourceResponse>(j_response);
}

}  // namespace android_webview
