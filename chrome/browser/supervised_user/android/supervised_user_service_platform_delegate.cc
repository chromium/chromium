// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/supervised_user_service_platform_delegate_jni_headers/SupervisedUserServicePlatformDelegate_jni.h"

SupervisedUserServicePlatformDelegate::SupervisedUserServicePlatformDelegate(
    Profile& profile)
    : ChromeSupervisedUserServicePlatformDelegateBase(profile) {}

void SupervisedUserServicePlatformDelegate::CloseIncognitoTabs() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SupervisedUserServicePlatformDelegate_closeIncognitoTabs(env);
}
