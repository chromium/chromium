// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/permission/aw_permission_request.h"

#include <utility>

#include "android_webview/browser/permission/aw_permission_request_delegate.h"
#include "android_webview/browser_jni_headers/AwPermissionRequest_jni.h"
#include "base/android/jni_string.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

// static
base::android::ScopedJavaLocalRef<jobject> AwPermissionRequest::Create(
    std::unique_ptr<AwPermissionRequestDelegate> delegate,
    base::WeakPtr<AwPermissionRequest>* weak_ptr) {
  base::android::ScopedJavaLocalRef<jobject> java_peer;
  AwPermissionRequest* permission_request =
      new AwPermissionRequest(std::move(delegate), &java_peer);
  *weak_ptr = permission_request->weak_factory_.GetWeakPtr();
  return java_peer;
}

AwPermissionRequest::AwPermissionRequest(
    std::unique_ptr<AwPermissionRequestDelegate> delegate,
    ScopedJavaLocalRef<jobject>* java_peer)
    : delegate_(std::move(delegate)), processed_(false) {
  DCHECK(delegate_.get());
  DCHECK(java_peer);

  JNIEnv* env = AttachCurrentThread();
  *java_peer = Java_AwPermissionRequest_create(
      env, reinterpret_cast<jlong>(this),
      ConvertUTF8ToJavaString(env, GetOrigin().spec()), GetResources());
  java_ref_ = JavaObjectWeakGlobalRef(env, java_peer->obj());
}

AwPermissionRequest::~AwPermissionRequest() {
  OnAcceptInternal(false);
}

void AwPermissionRequest::OnAccept(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller,
                                   jboolean accept) {
  OnAcceptInternal(accept);
}

void AwPermissionRequest::OnAcceptInternal(bool accept) {
  if (!processed_) {
    delegate_->NotifyRequestResult(accept);
    processed_ = true;
  }
}

void AwPermissionRequest::DeleteThis() {
  ScopedJavaLocalRef<jobject> j_request = GetJavaObject();
  if (j_request.is_null())
    return;
  Java_AwPermissionRequest_destroyNative(AttachCurrentThread(), j_request);
}

void AwPermissionRequest::Destroy(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<jobject> AwPermissionRequest::GetJavaObject() {
  return java_ref_.get(AttachCurrentThread());
}

const GURL& AwPermissionRequest::GetOrigin() {
  return delegate_->GetOrigin();
}

int64_t AwPermissionRequest::GetResources() {
  return delegate_->GetResources();
}

void AwPermissionRequest::CancelAndDelete() {
  processed_ = true;
  DeleteThis();
}

}  // namespace android_webview
