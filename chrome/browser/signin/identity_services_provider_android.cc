// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/services/android/jni_headers/IdentityServicesProvider_jni.h"
#include "chrome/browser/signin/signin_manager_android_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetIdentityManager(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // Ensuring that the pointer is not null here produces unactionable stack
  // traces, so just let the Java side handle possible issues with null.
  return identity_manager ? identity_manager->GetJavaObject() : nullptr;
}

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetAccountTrackerService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // Ensuring that the pointer is not null here produces unactionable stack
  // traces, so just let the Java side handle possible issues with null.
  return identity_manager
             ? identity_manager->LegacyGetAccountTrackerServiceJavaObject()
             : nullptr;
}

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetSigninManager(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  SigninManagerAndroid* signin_manager =
      SigninManagerAndroidFactory::GetForProfile(profile);
  // Ensuring that the pointer is not null here produces unactionable stack
  // traces, so just let the Java side handle possible issues with null.
  return signin_manager ? signin_manager->GetJavaObject() : nullptr;
}
