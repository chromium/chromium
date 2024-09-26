// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_manager_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_android_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/IdentityServicesProvider_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetIdentityManager(JNIEnv* env, Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // Ensuring that the pointer is not null here produces unactionable stack
  // traces, so just let the Java side handle possible issues with null.
  return identity_manager ? identity_manager->GetJavaObject() : nullptr;
}

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetSigninManager(JNIEnv* env, Profile* profile) {
  SigninManagerAndroid* signin_manager =
      SigninManagerAndroidFactory::GetForProfile(profile);
  // Ensuring that the pointer is not null here produces unactionable stack
  // traces, so just let the Java side handle possible issues with null.
  return signin_manager ? signin_manager->GetJavaObject() : nullptr;
}
