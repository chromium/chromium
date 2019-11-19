// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/accessibility/font_size_prefs_android.h"

#include "base/bind.h"
#include "base/observer_list.h"
#include "chrome/android/chrome_jni_headers/FontSizePrefs_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

FontSizePrefsAndroid::FontSizePrefsAndroid(JNIEnv* env, jobject obj)
    : pref_service_(ProfileManager::GetActiveUserProfile()->GetPrefs()) {
  java_ref_.Reset(env, obj);
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kWebKitFontScaleFactor,
      base::Bind(&FontSizePrefsAndroid::OnFontScaleFactorChanged,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kWebKitForceEnableZoom,
      base::Bind(&FontSizePrefsAndroid::OnForceEnableZoomChanged,
                 base::Unretained(this)));
}

FontSizePrefsAndroid::~FontSizePrefsAndroid() {
}

void FontSizePrefsAndroid::SetFontScaleFactor(JNIEnv* env,
                                              const JavaRef<jobject>& obj,
                                              jfloat font_size) {
  pref_service_->SetDouble(prefs::kWebKitFontScaleFactor,
                           static_cast<double>(font_size));
}

float FontSizePrefsAndroid::GetFontScaleFactor(
    JNIEnv* env,
    const JavaRef<jobject>& obj) {
  return pref_service_->GetDouble(prefs::kWebKitFontScaleFactor);
}

void FontSizePrefsAndroid::SetForceEnableZoom(JNIEnv* env,
                                              const JavaRef<jobject>& obj,
                                              jboolean enabled) {
  pref_service_->SetBoolean(prefs::kWebKitForceEnableZoom, enabled);
}

bool FontSizePrefsAndroid::GetForceEnableZoom(
    JNIEnv* env,
    const JavaRef<jobject>& obj) {
  return pref_service_->GetBoolean(prefs::kWebKitForceEnableZoom);
}

jlong JNI_FontSizePrefs_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  FontSizePrefsAndroid* font_size_prefs_android =
      new FontSizePrefsAndroid(env, obj);
  return reinterpret_cast<intptr_t>(font_size_prefs_android);
}

void FontSizePrefsAndroid::OnFontScaleFactorChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  float factor = GetFontScaleFactor(env, java_ref_);
  Java_FontSizePrefs_onFontScaleFactorChanged(env, java_ref_, factor);
}

void FontSizePrefsAndroid::OnForceEnableZoomChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool enabled = GetForceEnableZoom(env, java_ref_);
  Java_FontSizePrefs_onForceEnableZoomChanged(env, java_ref_, enabled);
}
