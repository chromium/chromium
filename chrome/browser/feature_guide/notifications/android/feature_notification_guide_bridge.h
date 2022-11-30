// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_ANDROID_FEATURE_NOTIFICATION_GUIDE_BRIDGE_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_ANDROID_FEATURE_NOTIFICATION_GUIDE_BRIDGE_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/supports_user_data.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace feature_guide {
class FeatureNotificationGuideService;

// Contains JNI methods needed by the feature notification guide.
class FeatureNotificationGuideBridge
    : public base::SupportsUserData::Data,
      public FeatureNotificationGuideService::Delegate {
 public:
  FeatureNotificationGuideBridge();
  ~FeatureNotificationGuideBridge() override;

  ScopedJavaLocalRef<jobject> GetJavaObj();
  std::u16string GetNotificationTitle(FeatureType feature) override;
  std::u16string GetNotificationMessage(FeatureType feature) override;
  void OnNotificationClick(FeatureType feature) override;
  void CloseNotification(const std::string& notification_guid) override;
  bool ShouldSkipFeature(FeatureType feature) override;
  std::string GetNotificationParamGuidForFeature(FeatureType feature) override;

 private:
  // A reference to the Java counterpart of this class.  See
  // FeatureNotificationGuideBridge.java.
  ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_ANDROID_FEATURE_NOTIFICATION_GUIDE_BRIDGE_H_
