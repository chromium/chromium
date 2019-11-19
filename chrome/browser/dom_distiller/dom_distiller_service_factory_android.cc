// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/DomDistillerServiceFactory_jni.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/dom_distiller/core/dom_distiller_service_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace dom_distiller {
namespace android {

ScopedJavaLocalRef<jobject> DomDistillerServiceFactoryAndroid::GetForProfile(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile) {
  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          ProfileAndroid::FromProfileAndroid(j_profile));
  DomDistillerServiceAndroid* service_android =
      new DomDistillerServiceAndroid(service);
  return ScopedJavaLocalRef<jobject>(service_android->java_ref_);
}

ScopedJavaLocalRef<jobject> JNI_DomDistillerServiceFactory_GetForProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return DomDistillerServiceFactoryAndroid::GetForProfile(env, j_profile);
}

}  // namespace android
}  // namespace dom_distiller
