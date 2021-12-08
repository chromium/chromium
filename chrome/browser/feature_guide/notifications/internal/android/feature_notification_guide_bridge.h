// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_ANDROID_FEATURE_NOTIFICATION_GUIDE_BRIDGE_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_ANDROID_FEATURE_NOTIFICATION_GUIDE_BRIDGE_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace feature_guide {
class FeatureNotificationGuideService;

// Contains JNI methods needed by the feature notification guide.
class FeatureNotificationGuideBridge : public base::SupportsUserData::Data {
 public:
  // Returns a Java FeatureNotificationGuideBridge for |service|.
  // There will be only one bridge per FeatureNotificationGuideBridge.
  static FeatureNotificationGuideBridge* GetFeatureNotificationGuideBridge(
      FeatureNotificationGuideService* feature_notification_guide_service);

  explicit FeatureNotificationGuideBridge(
      FeatureNotificationGuideService* feature_notification_guide_service);
  ~FeatureNotificationGuideBridge() override;

  ScopedJavaLocalRef<jobject> GetJavaObj();
  std::u16string GetNotificationTitle(FeatureType feature);
  std::u16string GetNotificationMessage(FeatureType feature);
  void OnNotificationClick(FeatureType feature);

 private:
  // A reference to the Java counterpart of this class.  See
  // FeatureNotificationGuideBridge.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<FeatureNotificationGuideService> feature_notification_guide_service_;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_ANDROID_FEATURE_NOTIFICATION_GUIDE_BRIDGE_H_
