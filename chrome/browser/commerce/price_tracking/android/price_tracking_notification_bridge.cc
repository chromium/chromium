// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/price_tracking/android/price_tracking_notification_bridge.h"

#include "base/android/jni_array.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/commerce/price_tracking/android/jni_headers/PriceTrackingNotificationBridge_jni.h"
#include "content/public/browser/browser_context.h"

using OptimizationType = optimization_guide::proto::OptimizationType;

const char kUserDataKey[] = "commerce.price_tracking_notification_bridge";

// static
PriceTrackingNotificationBridge*
PriceTrackingNotificationBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!context->GetUserData(kUserDataKey)) {
    context->SetUserData(kUserDataKey,
                         base::WrapUnique(new PriceTrackingNotificationBridge));
  }
  return static_cast<PriceTrackingNotificationBridge*>(
      context->GetUserData(kUserDataKey));
}

PriceTrackingNotificationBridge::PriceTrackingNotificationBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_PriceTrackingNotificationBridge_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

PriceTrackingNotificationBridge::~PriceTrackingNotificationBridge() = default;

void PriceTrackingNotificationBridge::OnNotificationPayload(
    optimization_guide::proto::OptimizationType optimization_type,
    const optimization_guide::proto::Any& payload) {
  // Only parse PRICE_TRACKING payload.
  if (optimization_type != OptimizationType::PRICE_TRACKING ||
      !payload.has_value()) {
    return;
  }

  // Pass the payload to Java.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PriceTrackingNotificationBridge_showNotification(
      env, java_obj_, base::android::ToJavaByteArray(env, payload.value()));
}
