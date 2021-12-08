// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"
#include "chrome/browser/feature_guide/notifications/internal/android/feature_notification_guide_bridge.h"
#include "chrome/browser/feature_guide/notifications/internal/jni_headers/FeatureNotificationGuideServiceFactory_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_FeatureNotificationGuideServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  feature_guide::FeatureNotificationGuideService* service =
      feature_guide::FeatureNotificationGuideServiceFactory::GetForProfile(
          profile->GetOriginalProfile());
  return feature_guide::FeatureNotificationGuideBridge::
      GetFeatureNotificationGuideBridge(service)
          ->GetJavaObj();
}
