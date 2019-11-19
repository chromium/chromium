// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/pref_change_registrar_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/PrefChangeRegistrar_jni.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/browser/profiles/profile_manager.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

PrefChangeRegistrarAndroid::PrefChangeRegistrarAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  profile_ = ProfileManager::GetActiveUserProfile()->GetOriginalProfile();

  pref_change_registrar_.Init(profile_->GetPrefs());

  pref_change_registrar_jobject_.Reset(env, obj);
}

PrefChangeRegistrarAndroid::~PrefChangeRegistrarAndroid() {}

void PrefChangeRegistrarAndroid::Destroy(JNIEnv*,
                                         const JavaParamRef<jobject>&) {
  delete this;
}

void PrefChangeRegistrarAndroid::Add(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     const jint j_pref_index) {
  pref_change_registrar_.Add(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index),
      base::Bind(&PrefChangeRegistrarAndroid::OnPreferenceChange,
                 base::Unretained(this), j_pref_index));
}

void PrefChangeRegistrarAndroid::Remove(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const jint j_pref_index) {
  pref_change_registrar_.Remove(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index));
}

void PrefChangeRegistrarAndroid::OnPreferenceChange(const int pref_index) {
  JNIEnv* env = AttachCurrentThread();
  Java_PrefChangeRegistrar_onPreferenceChange(
      env, pref_change_registrar_jobject_, pref_index);
}

jlong JNI_PrefChangeRegistrar_Init(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new PrefChangeRegistrarAndroid(env, obj));
}
