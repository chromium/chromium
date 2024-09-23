// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_PRICE_TRACKING_ANDROID_PRICE_TRACKING_NOTIFICATION_BRIDGE_H_
#define CHROME_BROWSER_COMMERCE_PRICE_TRACKING_ANDROID_PRICE_TRACKING_NOTIFICATION_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/supports_user_data.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/push_notification.pb.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

// JNI bridge that receives the price tracking notification payload from
// optimization_guide::PushNotificationManager. This class is owned by a browser
// context through SupportsUserData.
class PriceTrackingNotificationBridge
    : public optimization_guide::PushNotificationManager::Observer,
      public base::SupportsUserData::Data {
 public:
  // Get the bridge from a browser context.
  static PriceTrackingNotificationBridge* GetForBrowserContext(
      content::BrowserContext* context);
  ~PriceTrackingNotificationBridge() override;

  // optimization_guide::PushNotificationManager::Observer implementation.
  void OnNotificationPayload(
      optimization_guide::proto::OptimizationType optimization_type,
      const optimization_guide::proto::Any& payload) override;

 private:
  explicit PriceTrackingNotificationBridge(Profile* profile);

  // The Java object, owned by the native object.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

#endif  // CHROME_BROWSER_COMMERCE_PRICE_TRACKING_ANDROID_PRICE_TRACKING_NOTIFICATION_BRIDGE_H_
