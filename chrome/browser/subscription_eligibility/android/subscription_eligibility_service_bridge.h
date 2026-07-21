// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_ANDROID_SUBSCRIPTION_ELIGIBILITY_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_ANDROID_SUBSCRIPTION_ELIGIBILITY_SERVICE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"

namespace subscription_eligibility {

// JNI bridge to surface SubscriptionEligibilityService to Java.
class SubscriptionEligibilityServiceBridge
    : public SubscriptionEligibilityService::Observer {
 public:
  SubscriptionEligibilityServiceBridge(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& java_ref,
      SubscriptionEligibilityService* service);

  SubscriptionEligibilityServiceBridge(
      const SubscriptionEligibilityServiceBridge&) = delete;
  SubscriptionEligibilityServiceBridge& operator=(
      const SubscriptionEligibilityServiceBridge&) = delete;

  ~SubscriptionEligibilityServiceBridge() override;

  // Called from Java.
  void Destroy(JNIEnv* env);
  int32_t GetAiSubscriptionTier(JNIEnv* env);

 private:
  // SubscriptionEligibilityService::Observer:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  base::WeakPtr<SubscriptionEligibilityService> service_;
};

}  // namespace subscription_eligibility

#endif  // CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_ANDROID_SUBSCRIPTION_ELIGIBILITY_SERVICE_BRIDGE_H_
