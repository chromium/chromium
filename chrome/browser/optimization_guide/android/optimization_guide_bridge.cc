// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"

#include <jni.h>

#include <string>
#include <typeinfo>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_view_util.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/hints/hint_cache.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_store.h"
#include "components/optimization_guide/core/hints/push_notification_manager.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/optimization_guide/android/jni_headers/OptimizationGuideBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaArrayOfByteArrayToBytesVector;
using base::android::JavaByteArrayToString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace optimization_guide {
namespace android {

namespace {

ScopedJavaLocalRef<jbyteArray> ToJavaSerializedAnyMetadata(
    JNIEnv* env,
    const optimization_guide::OptimizationMetadata& optimization_metadata) {
  // We do not expect the following metadatas to be populated for optimization
  // types getting called from Java.
  DCHECK(!optimization_metadata.loading_predictor_metadata());

  if (optimization_metadata.any_metadata()) {
    std::string serialized;
    optimization_metadata.any_metadata().value().SerializeToString(&serialized);
    return ToJavaByteArray(env, serialized);
  }
  return nullptr;
}

void OnOptimizationGuideDecision(
    const JavaRef<jobject>& java_callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  JNIEnv* env = AttachCurrentThread();
  Java_OptimizationGuideBridge_onOptimizationGuideDecision(
      env, java_callback, static_cast<int>(decision),
      ToJavaSerializedAnyMetadata(env, metadata));
}

base::flat_set<proto::OptimizationType> ToOptTypesSet(
    const std::vector<int32_t>& optimization_types) {
  base::flat_set<optimization_guide::proto::OptimizationType> opt_types;
  for (const int32_t optimization_type : optimization_types) {
    // Handles parsing of reserved tag numbers.
    if (proto::OptimizationType_IsValid(optimization_type)) {
      opt_types.insert(static_cast<proto::OptimizationType>(optimization_type));
    }
  }
  return opt_types;
}

void OnOnDemandOptimizationGuideDecision(
    const JavaRef<jobject>& java_callback,
    const GURL& url,
    const base::flat_map<proto::OptimizationType,
                         OptimizationGuideDecisionWithMetadata>& metadata) {
  JNIEnv* env = AttachCurrentThread();
  for (const auto& type_and_decision : metadata) {
    Java_OptimizationGuideBridge_onOnDemandOptimizationGuideDecision(
        env, java_callback, url::GURLAndroid::FromNativeGURL(env, url),
        static_cast<int>(type_and_decision.first),
        static_cast<int>(type_and_decision.second.decision),
        ToJavaSerializedAnyMetadata(env, type_and_decision.second.metadata));
  }
}

}  // namespace

// static
std::vector<proto::HintNotificationPayload>
OptimizationGuideBridge::GetCachedNotifications(
    proto::OptimizationType optimization_type) {
  JNIEnv* env = AttachCurrentThread();
  const JavaRef<jobjectArray>& j_encoded_notifications =
      Java_OptimizationGuideBridge_getEncodedPushNotifications(
          env, static_cast<int>(optimization_type));
  if (!j_encoded_notifications)
    return {};

  std::vector<std::vector<uint8_t>> encoded_notifications;
  JavaArrayOfByteArrayToBytesVector(env, j_encoded_notifications,
                                    &encoded_notifications);

  std::vector<proto::HintNotificationPayload> notifications;
  for (const auto& encoded_notification : encoded_notifications) {
    proto::HintNotificationPayload notification;
    if (notification.ParseFromString(
            base::as_string_view(encoded_notification))) {
      notifications.push_back(std::move(notification));
    }
  }

  return notifications;
}

// static
base::flat_set<proto::OptimizationType>
OptimizationGuideBridge::GetOptTypesWithPushNotifications() {
  JNIEnv* env = AttachCurrentThread();
  return ToOptTypesSet(
      Java_OptimizationGuideBridge_getOptTypesWithPushNotifications(env));
}

// static
base::flat_set<proto::OptimizationType>
OptimizationGuideBridge::GetOptTypesThatOverflowedPushNotifications() {
  JNIEnv* env = AttachCurrentThread();
  return ToOptTypesSet(
      Java_OptimizationGuideBridge_getOptTypesThatOverflowedPushNotifications(
          env));
}

// static
void OptimizationGuideBridge::ClearCacheForOptimizationType(
    proto::OptimizationType opt_type) {
  JNIEnv* env = AttachCurrentThread();
  Java_OptimizationGuideBridge_clearCachedPushNotifications(
      env, static_cast<int>(opt_type));
}

// static
void OptimizationGuideBridge::OnNotificationNotHandledByNative(
    proto::HintNotificationPayload notification) {
  std::string encoded_notification;
  if (!notification.SerializeToString(&encoded_notification))
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_OptimizationGuideBridge_onPushNotificationNotHandledByNative(
      env, ToJavaByteArray(env, encoded_notification));
}

void OptimizationGuideBridge::OnDeferredStartup(JNIEnv* env) {
  optimization_guide_keyed_service_->GetHintsManager()->OnDeferredStartup();
}

OptimizationGuideBridge::OptimizationGuideBridge(
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : optimization_guide_keyed_service_(optimization_guide_keyed_service) {
  DCHECK(optimization_guide_keyed_service_);
}

OptimizationGuideBridge::~OptimizationGuideBridge() = default;

ScopedJavaLocalRef<JOptimizationGuideBridge>
OptimizationGuideBridge::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  if (!java_ref_) {
    java_ref_.Reset(
        OptimizationGuideBridgeJni::New(env, reinterpret_cast<intptr_t>(this)));
  }
  return ScopedJavaLocalRef<JOptimizationGuideBridge>(java_ref_);
}

void OptimizationGuideBridge::RegisterOptimizationTypes(
    const std::vector<int32_t>& optimization_types) {
  base::flat_set<proto::OptimizationType> opt_types_set =
      ToOptTypesSet(optimization_types);
  optimization_guide_keyed_service_->RegisterOptimizationTypes(
      {opt_types_set.begin(), opt_types_set.end()});
}

void OptimizationGuideBridge::CanApplyOptimization(
    JNIEnv* env,
    const GURL& url,
    int32_t optimization_type,
    const JavaRef<jobject>& java_callback) {
  optimization_guide_keyed_service_->CanApplyOptimization(
      url,
      static_cast<optimization_guide::proto::OptimizationType>(
          optimization_type),
      base::BindOnce(&OnOptimizationGuideDecision,
                     ScopedJavaGlobalRef<jobject>(env, java_callback)));
}

base::android::ScopedJavaLocalRef<jobject>
OptimizationGuideBridge::CanApplyOptimizationSync(JNIEnv* env,
                                                  const GURL& url,
                                                  int32_t optimization_type) {
  optimization_guide::OptimizationMetadata metadata;

  auto decision = optimization_guide_keyed_service_->CanApplyOptimization(
      url,
      static_cast<optimization_guide::proto::OptimizationType>(
          optimization_type),
      /* optimization_metadata = */ &metadata);

  return Java_OptimizationGuideBridge_createDecisionWithMetadata(
      env, static_cast<int>(decision),
      ToJavaSerializedAnyMetadata(env, metadata));
}

void OptimizationGuideBridge::CanApplyOptimizationOnDemand(
    JNIEnv* env,
    const std::vector<GURL>& urls,
    const std::vector<int32_t>& optimization_types,
    int32_t request_context,
    const JavaRef<jobject>& java_callback,
    const JavaRef<JArray<int8_t>>& request_context_metadata_serialized) {
  std::optional<optimization_guide::proto::RequestContextMetadata>
      request_context_metadata;
  {
    jni_zero::JArrayViewCritical<int8_t> serialized_view =
        request_context_metadata_serialized.CreateViewCritical(env);
    if (!serialized_view.empty()) {
      proto::RequestContextMetadata request_context_metadata_deserialized;
      request_context_metadata_deserialized.ParseFromArray(
          reinterpret_cast<const uint8_t*>(serialized_view.data()),
          serialized_view.size());
      request_context_metadata =
          std::move(request_context_metadata_deserialized);
    }
  }

  optimization_guide_keyed_service_->CanApplyOptimizationOnDemand(
      urls, ToOptTypesSet(optimization_types),
      static_cast<proto::RequestContext>(request_context),
      base::BindRepeating(&OnOnDemandOptimizationGuideDecision,
                          ScopedJavaGlobalRef<jobject>(env, java_callback)),
      request_context_metadata);
}

void OptimizationGuideBridge::OnNewPushNotification(
    JNIEnv* env,
    const JavaRef<jbyteArray>& j_encoded_notification) {
  if (!j_encoded_notification)
    return;

  proto::HintNotificationPayload notification;
  std::string encoded_notification;
  JavaByteArrayToString(env, j_encoded_notification, &encoded_notification);
  if (!notification.ParseFromString(encoded_notification))
    return;

  if (!notification.has_hint_key())
    return;

  optimization_guide::ChromeHintsManager* hints_manager =
      optimization_guide_keyed_service_->GetHintsManager();
  PushNotificationManager* push_manager =
      hints_manager ? hints_manager->push_notification_manager() : nullptr;
  if (!push_manager)
    return;

  push_manager->OnNewPushNotification(notification);
}

}  // namespace android
}  // namespace optimization_guide

DEFINE_JNI(OptimizationGuideBridge)
