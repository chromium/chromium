// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_network_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/android/chrome_jni_headers/FeedNetworkBridge_jni.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_networking_host.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

namespace feed {

FeedNetworkBridge::FeedNetworkBridge(const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  networking_host_ = host_service->GetNetworkingHost();
  DCHECK(networking_host_);
}

FeedNetworkBridge::~FeedNetworkBridge() = default;

static jlong JNI_FeedNetworkBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  return reinterpret_cast<intptr_t>(new FeedNetworkBridge(j_profile));
}

void FeedNetworkBridge::Destroy(JNIEnv* env,
                                const JavaParamRef<jobject>& j_this) {
  delete this;
}

void FeedNetworkBridge::SendNetworkRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_request_type,
    const JavaParamRef<jbyteArray>& j_body,
    const JavaParamRef<jobject>& j_callback) {
  auto url = GURL(ConvertJavaStringToUTF8(env, j_url));
  FeedNetworkingHost::ResponseCallback callback =
      base::BindOnce(&FeedNetworkBridge::OnResult, weak_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(env, j_callback));
  std::vector<uint8_t> request_body;
  base::android::JavaByteArrayToByteVector(env, j_body, &request_body);

  networking_host_->Send(url, ConvertJavaStringToUTF8(env, j_request_type),
                         std::move(request_body), std::move(callback));
}

void FeedNetworkBridge::CancelRequests(JNIEnv* env,
                                       const JavaParamRef<jobject>& j_this) {
  networking_host_->CancelRequests();
}

void FeedNetworkBridge::OnResult(const ScopedJavaGlobalRef<jobject>& j_callback,
                                 int32_t http_code,
                                 std::vector<uint8_t> response_bytes) {
  // TODO(ssid): Remove after fixing https://crbug.com/916791.
  TRACE_EVENT0("browser", "FeedNetworkBridge::OnResult");
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response_bytes =
      base::android::ToJavaByteArray(env, response_bytes);
  ScopedJavaLocalRef<jobject> j_http_response =
      Java_FeedNetworkBridge_createHttpResponse(env, http_code,
                                                j_response_bytes);
  TRACE_EVENT0("browser", "FeedNetworkBridge::OnResult_Callback");
  base::android::RunObjectCallbackAndroid(j_callback, j_http_response);
}

}  // namespace feed
