// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/segmentation_platform/jni_headers/SegmentationPlatformServiceFactory_jni.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_SegmentationPlatformServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  segmentation_platform::SegmentationPlatformService* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile);
  return segmentation_platform::SegmentationPlatformService::GetJavaObject(
      service);
}
