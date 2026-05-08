// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentRestrictionManagerBridge_jni.h"

namespace android_webview {

AwContentRestrictionManagerClient::AwContentRestrictionManagerClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_bridge_.Reset(Java_AwContentRestrictionManagerBridge_Constructor(env));
}

AwContentRestrictionManagerClient::~AwContentRestrictionManagerClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwContentRestrictionManagerBridge_destroy(env, java_bridge_);
}

bool AwContentRestrictionManagerClient::IsContentRestrictionEnabled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();

  // TODO(crbug.com/481115059): Cache the result of this call to avoid repeated
  // IPCs.
  return Java_AwContentRestrictionManagerBridge_isContentRestrictionEnabled(
      env, java_bridge_);
}

void AwContentRestrictionManagerClient::RequestContentClassification(
    int64_t navigation_id,
    const network::ResourceRequest& request,
    ContentClassificationCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string mime_type;
  auto header_value =
      request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
  if (header_value) {
    mime_type = *header_value;
  }

  Java_AwContentRestrictionManagerBridge_requestContentClassification(
      env, java_bridge_, navigation_id, request.url.spec(), mime_type,
      base::android::ToJniCallback(env, std::move(callback)));
}

bool AwContentRestrictionManagerClient::SendShowRestrictedContentIntent(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwContentRestrictionManagerBridge_sendShowRestrictedContentIntent(
      env, java_bridge_, url.spec());
}

int AwContentRestrictionManagerClient::CreateRequestBodyPipeAndGetWriteFd(
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwContentRestrictionManagerBridge_createRequestBodyPipeAndGetWriteFd(
      env, java_bridge_, navigation_id);
}

}  // namespace android_webview
