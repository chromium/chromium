// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/android/policy_service_android.h"
#include "components/policy/core/common/policy_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/policy/android/jni_headers/PolicyServiceFactory_jni.h"

namespace policy {
namespace android {

base::android::ScopedJavaLocalRef<jobject>
JNI_PolicyServiceFactory_GetGlobalPolicyService(JNIEnv* env) {
  return g_browser_process->policy_service()
      ->GetPolicyServiceAndroid()
      ->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject>
JNI_PolicyServiceFactory_GetProfilePolicyService(JNIEnv* env,
                                                 Profile* profile) {
  DCHECK(profile);
  return profile->GetProfilePolicyConnector()
      ->policy_service()
      ->GetPolicyServiceAndroid()
      ->GetJavaObject();
}

}  // namespace android
}  // namespace policy
