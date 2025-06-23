// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"

// Include last. Requires declarations from includes above.
#include "chrome/browser/supervised_user/supervised_user_service_bridge_jni_headers/SupervisedUserServiceBridge_jni.h"

namespace supervised_user {
jboolean JNI_SupervisedUserServiceBridge_IsSupervisedLocally(
    JNIEnv* env,
    Profile* profile) {
  return SupervisedUserServiceFactory::GetForProfile(profile)
      ->IsSupervisedLocally();
}
}  // namespace supervised_user
