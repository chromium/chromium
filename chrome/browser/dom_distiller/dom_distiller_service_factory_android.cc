// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/core/dom_distiller_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DomDistillerServiceFactory_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace dom_distiller {
namespace android {

ScopedJavaLocalRef<jobject> DomDistillerServiceFactoryAndroid::GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(profile);
  DomDistillerServiceAndroid* service_android =
      new DomDistillerServiceAndroid(service);
  return ScopedJavaLocalRef<jobject>(service_android->java_ref_);
}

ScopedJavaLocalRef<jobject> JNI_DomDistillerServiceFactory_GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  return DomDistillerServiceFactoryAndroid::GetForProfile(env, profile);
}

}  // namespace android
}  // namespace dom_distiller
