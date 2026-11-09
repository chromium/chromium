// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reader_mode/reader_mode_service_factory_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reader_mode/reader_mode_service_factory.h"
#include "components/web_distiller/core/web_distiller_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ReaderModeServiceFactory_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace reader_mode::android {

ScopedJavaLocalRef<jobject> ReaderModeServiceFactoryAndroid::GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  web_distiller::WebDistillerService* service =
      reader_mode::ReaderModeServiceFactory::GetForBrowserContext(profile);
  web_distiller::android::WebDistillerServiceAndroid* service_android =
      new web_distiller::android::WebDistillerServiceAndroid(service);
  return ScopedJavaLocalRef<jobject>(service_android->java_ref_);
}

static ScopedJavaLocalRef<jobject> JNI_ReaderModeServiceFactory_GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  return ReaderModeServiceFactoryAndroid::GetForProfile(env, profile);
}

}  // namespace reader_mode::android

DEFINE_JNI(ReaderModeServiceFactory)
