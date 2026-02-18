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
  base::android::ScopedJavaLocalRef<JLocaleManager> jlocale_manager =
      JLocaleManagerClass::getInstance(env);
  if (jlocale_manager.is_null())
    return "";
  return jlocale_manager->getYandexReferralId(env);
}

// static
std::string LocaleManager::GetMailRUReferralID() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<JLocaleManager> jlocale_manager =
      JLocaleManagerClass::getInstance(env);
  if (jlocale_manager.is_null())
    return "";
  return jlocale_manager->getMailRUReferralId(env);
}

DEFINE_JNI(LocaleManager)
