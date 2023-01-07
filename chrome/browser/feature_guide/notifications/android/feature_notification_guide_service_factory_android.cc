// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/FeatureNotificationGuideServiceFactory_jni.h"
#include "chrome/browser/feature_guide/notifications/android/feature_notification_guide_bridge.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"
#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_FeatureNotificationGuideServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  feature_guide::FeatureNotificationGuideServiceImpl* service =
      static_cast<feature_guide::FeatureNotificationGuideServiceImpl*>(
          feature_guide::FeatureNotificationGuideServiceFactory::GetForProfile(
              profile->GetOriginalProfile()));
  return static_cast<feature_guide::FeatureNotificationGuideBridge*>(
             service->GetDelegate())
      ->GetJavaObj();
}
