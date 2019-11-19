// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_preference_helper.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ContextualSearchPreferenceHelper_jni.h"
#include "components/contextual_search/core/browser/contextual_search_preference.h"

ContextualSearchPreferenceHelper::ContextualSearchPreferenceHelper(
    JNIEnv* env,
    jobject obj) {
  java_object_.Reset(env, obj);
}

ContextualSearchPreferenceHelper::~ContextualSearchPreferenceHelper() {}

jint ContextualSearchPreferenceHelper::GetPreferenceMetadata(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ContextualSearchPreviousPreferenceMetadata metadata =
      contextual_search::ContextualSearchPreference::GetInstance()
          ->GetPreviousPreferenceMetadata();
  return static_cast<jint>(metadata);
}

// Java wrapper boilerplate

void ContextualSearchPreferenceHelper::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_ContextualSearchPreferenceHelper_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ContextualSearchPreferenceHelper* helper =
      new ContextualSearchPreferenceHelper(env, obj);
  return reinterpret_cast<intptr_t>(helper);
}
