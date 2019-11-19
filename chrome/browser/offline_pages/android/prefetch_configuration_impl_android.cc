// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/PrefetchConfiguration_jni.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"

using base::android::JavaParamRef;

// These functions fulfill the Java to native link between
// PrefetchConfiguration.java and prefetch_prefs.

namespace offline_pages {
namespace android {

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsPrefetchingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  return static_cast<jboolean>(prefetch_prefs::IsEnabled(key->GetPrefs()));
}

JNI_EXPORT jboolean
JNI_PrefetchConfiguration_IsEnabledByServer(JNIEnv* env,
                                            const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  return static_cast<jboolean>(
      prefetch_prefs::IsEnabledByServer(key->GetPrefs()));
}

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsForbiddenCheckDue(
    JNIEnv* env,
    const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  return static_cast<jboolean>(
      prefetch_prefs::IsForbiddenCheckDue(key->GetPrefs()));
}

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsEnabledByServerUnknown(
    JNIEnv* env,
    const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  return static_cast<jboolean>(
      prefetch_prefs::IsEnabledByServerUnknown(key->GetPrefs()));
}

JNI_EXPORT void JNI_PrefetchConfiguration_SetPrefetchingEnabledInSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& jkey,
    jboolean enabled) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  prefetch_prefs::SetPrefetchingEnabledInSettings(key->GetPrefs(), enabled);
}

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsPrefetchingEnabledInSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  return static_cast<jboolean>(
      prefetch_prefs::IsPrefetchingEnabledInSettings(key->GetPrefs()));
}

}  // namespace android
}  // namespace offline_pages
