// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/locale_manager.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/locale/jni_headers/LocaleManager_jni.h"

// static
std::string LocaleManager::GetYandexReferralID() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlocale_manager =
      Java_LocaleManager_getInstance(env);
  if (jlocale_manager.is_null())
    return "";
  return base::android::ConvertJavaStringToUTF8(
      env, Java_LocaleManager_getYandexReferralId(env, jlocale_manager));
}

// static
std::string LocaleManager::GetMailRUReferralID() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlocale_manager =
      Java_LocaleManager_getInstance(env);
  if (jlocale_manager.is_null())
    return "";
  return base::android::ConvertJavaStringToUTF8(
      env, Java_LocaleManager_getMailRUReferralId(env, jlocale_manager));
}
