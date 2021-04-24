// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/segmentation_platform/jni_headers/SegmentationPlatformServiceFactory_jni.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_SegmentationPlatformServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  // TODO(shaktisahu): Use native factory to get the object and its java
  // counterpart from user data.
  return base::android::ScopedJavaLocalRef<jobject>();
}
