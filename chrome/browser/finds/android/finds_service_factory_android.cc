// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/finds_service_factory.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/finds/android/finds_service_android.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/finds/android/jni_headers/FindsServiceFactory_jni.h"

namespace {
const char kFindsServiceAndroidKey[] = "finds_service_android";
}

namespace finds {

// static
static jni_zero::ScopedJavaLocalRef<jobject>
JNI_FindsServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  finds::FindsService* service =
      finds::FindsServiceFactory::GetForProfile(profile);
  if (!service) {
    return nullptr;
  }
  finds::FindsServiceAndroid* android_service =
      static_cast<finds::FindsServiceAndroid*>(
          service->GetUserData(kFindsServiceAndroidKey));
  if (!android_service) {
    android_service = new finds::FindsServiceAndroid(service);
    service->SetUserData(kFindsServiceAndroidKey,
                         base::WrapUnique(android_service));
  }

  return android_service->GetJavaObject();
}

}  // namespace finds

DEFINE_JNI(FindsServiceFactory)
