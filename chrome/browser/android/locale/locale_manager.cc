// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/locale_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/LocaleManager_jni.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "url/gurl.h"

// static
std::string LocaleManager::GetYandexReferralID() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlocale_manager =
      Java_LocaleManager_getInstance(env);
  return base::android::ConvertJavaStringToUTF8(
      env, Java_LocaleManager_getYandexReferralId(env, jlocale_manager));
}

// static
std::string LocaleManager::GetMailRUReferralID() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlocale_manager =
      Java_LocaleManager_getInstance(env);
  return base::android::ConvertJavaStringToUTF8(
      env, Java_LocaleManager_getMailRUReferralId(env, jlocale_manager));
}

// static
void LocaleManager::RecordUserTypeMetrics() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlocale_manager =
      Java_LocaleManager_getInstance(env);
  return Java_LocaleManager_recordUserTypeMetrics(env, jlocale_manager);
}
