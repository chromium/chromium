// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/android/finds_service_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/finds/finds_service_factory.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/finds/android/jni_headers/FindsService_jni.h"

namespace finds {

FindsServiceAndroid::FindsServiceAndroid(FindsService* service)
    : service_(service) {
  service_->AddObserver(this);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_FindsService_create(env, reinterpret_cast<intptr_t>(this)));
}

FindsServiceAndroid::~FindsServiceAndroid() {
  service_->RemoveObserver(this);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_ref_) {
    Java_FindsService_clearNativePtr(env, java_ref_);
    java_ref_.Reset();
  }
}

void FindsServiceAndroid::OnOptInCriteriaFulfilled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FindsService_onOptInCriteriaFulfilled(env, GetJavaObject());
}

base::android::ScopedJavaLocalRef<jobject>
FindsServiceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

}  // namespace finds

DEFINE_JNI(FindsService)
