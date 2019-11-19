// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/IdentityServicesProvider_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_android_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetIdentityManager(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  return IdentityManagerFactory::GetForProfile(profile)->GetJavaObject();
}

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetAccountTrackerService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return identity_manager->LegacyGetAccountTrackerServiceJavaObject();
}

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetSigninManager(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  return SigninManagerAndroidFactory::GetJavaObjectForProfile(profile);
}
