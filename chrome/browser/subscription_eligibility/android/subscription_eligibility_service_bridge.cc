// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/android/subscription_eligibility_service_bridge.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/subscription_eligibility/jni_headers/SubscriptionEligibilityService_jni.h"

using jni_zero::JavaRef;

namespace subscription_eligibility {

int64_t JNI_SubscriptionEligibilityService_CreateBridge(
    JNIEnv* env,
    const JavaRef<jobject>& j_caller,
    const JavaRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  SubscriptionEligibilityService* service =
      SubscriptionEligibilityServiceFactory::GetForProfile(profile);

  // The Java object will manage the lifecycle of this C++ bridge.
  auto* bridge =
      new SubscriptionEligibilityServiceBridge(env, j_caller, service);
  return reinterpret_cast<intptr_t>(bridge);
}

SubscriptionEligibilityServiceBridge::SubscriptionEligibilityServiceBridge(
    JNIEnv* env,
    const JavaRef<jobject>& java_ref,
    SubscriptionEligibilityService* service)
    : java_ref_(env, java_ref),
      service_(service ? service->GetWeakPtr() : nullptr) {
  if (service_) {
    service_->AddObserver(this);
  }
}

SubscriptionEligibilityServiceBridge::~SubscriptionEligibilityServiceBridge() {
  if (service_) {
    service_->RemoveObserver(this);
  }
}

void SubscriptionEligibilityServiceBridge::Destroy(JNIEnv* env) {
  delete this;
}

int32_t SubscriptionEligibilityServiceBridge::GetAiSubscriptionTier(
    JNIEnv* env) {
  if (!service_) {
    return 0;
  }
  return service_->GetAiSubscriptionTier();
}

DEFINE_JNI_FOR_SubscriptionEligibilityService()

void SubscriptionEligibilityServiceBridge::OnAiSubscriptionTierUpdated(
    int32_t new_subscription_tier) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SubscriptionEligibilityService_onAiSubscriptionTierChanged(env,
                                                                  java_ref_);
}

}  // namespace subscription_eligibility
