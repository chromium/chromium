// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/android/jni_headers/GlicInstanceHelper_jni.h"  // nogncheck
#include "third_party/jni_zero/jni_zero.h"

namespace glic {

void GlicInstanceHelper::InitJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_ = base::android::ScopedJavaGlobalRef<jobject>(
      Java_GlicInstanceHelper_Constructor(env));
}

void GlicInstanceHelper::NotifyJavaInstanceTitleChanged() {
  if (java_ref_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    std::string conversation_id =
        bound_instance_ ? bound_instance_->conversation_id().value_or("") : "";
    std::string conversation_title =
        bound_instance_ ? bound_instance_->conversation_title() : "";

    Java_GlicInstanceHelper_onInstanceChanged(
        env, java_ref_,
        base::android::ConvertUTF8ToJavaString(env, conversation_id),
        base::android::ConvertUTF8ToJavaString(env, conversation_title));
  }
}

base::android::ScopedJavaLocalRef<jobject> GlicInstanceHelper::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_GlicInstanceHelper_GetForTab(JNIEnv* env,
                                 const jni_zero::JavaRef<jobject>& j_tab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab_android) {
    return nullptr;
  }
  GlicInstanceHelper* helper = GlicInstanceHelper::From(tab_android);
  if (!helper) {
    return nullptr;
  }
  return helper->GetJavaObject();
}

}  // namespace glic

DEFINE_JNI(GlicInstanceHelper)
