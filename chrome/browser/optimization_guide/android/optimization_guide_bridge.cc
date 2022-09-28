// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/optimization_guide/android/jni_headers/OptimizationGuideBridge_jni.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaArrayOfByteArrayToBytesVector;
using base::android::JavaByteArrayToString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace optimization_guide {
namespace android {

namespace {

ScopedJavaLocalRef<jbyteArray> ToJavaSerializedAnyMetadata(
    JNIEnv* env,
    const optimization_guide::OptimizationMetadata optimization_metadata) {
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
    if (notification.ParseFromString(std::string(encoded_notification.begin(),
                                                 encoded_notification.end()))) {
      notifications.push_back(notification);
    }
  }

  return notifications;
}

// static
base::flat_set<proto::OptimizationType>
OptimizationGuideBridge::GetOptTypesWithPushNotifications() {
  JNIEnv* env = AttachCurrentThread();
  std::vector<int> cached_int_types;
  JavaIntArrayToIntVector(
      env, Java_OptimizationGuideBridge_getOptTypesWithPushNotifications(env),
      &cached_int_types);

  base::flat_set<proto::OptimizationType> cached_types;
  for (int int_type : cached_int_types) {
    // Handles parsing of reserved tag numbers.
    if (proto::OptimizationType_IsValid(int_type)) {
      cached_types.insert(static_cast<proto::OptimizationType>(int_type));
    }
  }
  return cached_types;
}

// static
base::flat_set<proto::OptimizationType>
OptimizationGuideBridge::GetOptTypesThatOverflowedPushNotifications() {
  JNIEnv* env = AttachCurrentThread();
  std::vector<int> overflowed_int_types;
  JavaIntArrayToIntVector(
      env,
      Java_OptimizationGuideBridge_getOptTypesThatOverflowedPushNotifications(
          env),
      &overflowed_int_types);

  base::flat_set<proto::OptimizationType> overflowed_types;
  for (int int_type : overflowed_int_types) {
    // Handles parsing of reserved tag numbers.
    if (proto::OptimizationType_IsValid(int_type)) {
      overflowed_types.insert(static_cast<proto::OptimizationType>(int_type));
    }
  }
  return overflowed_types;
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

static jlong JNI_OptimizationGuideBridge_Init(JNIEnv* env) {
  // TODO(sophiechang): Figure out how to separate factory to avoid circular
  // deps when getting last used profile is no longer allowed.
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return 0;
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service)
    return 0;
  return reinterpret_cast<intptr_t>(
      new OptimizationGuideBridge(optimization_guide_keyed_service));
}

OptimizationGuideBridge::OptimizationGuideBridge(
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : optimization_guide_keyed_service_(optimization_guide_keyed_service) {
  DCHECK(optimization_guide_keyed_service_);
}

void OptimizationGuideBridge::Destroy(JNIEnv* env) {
  delete this;
}

void OptimizationGuideBridge::RegisterOptimizationTypes(
    JNIEnv* env,
    const JavaParamRef<jintArray>& joptimization_types) {
  // Convert optimization types to proto.
  std::vector<int> joptimization_types_vector;
  JavaIntArrayToIntVector(env, joptimization_types,
                          &joptimization_types_vector);
  std::vector<optimization_guide::proto::OptimizationType> optimization_types;
  for (const int joptimization_type : joptimization_types_vector) {
    optimization_types.push_back(
        static_cast<optimization_guide::proto::OptimizationType>(
            joptimization_type));
  }

  optimization_guide_keyed_service_->RegisterOptimizationTypes(
      optimization_types);
}

void OptimizationGuideBridge::CanApplyOptimizationAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_gurl,
    jint optimization_type,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK(optimization_guide_keyed_service_->GetHintsManager());
  optimization_guide_keyed_service_->GetHintsManager()
      ->CanApplyOptimizationAsync(
          *url::GURLAndroid::ToNativeGURL(env, java_gurl),
          static_cast<optimization_guide::proto::OptimizationType>(
              optimization_type),
          base::BindOnce(&OnOptimizationGuideDecision,
                         ScopedJavaGlobalRef<jobject>(env, java_callback)));
}

void OptimizationGuideBridge::CanApplyOptimization(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_gurl,
    jint optimization_type,
    const JavaParamRef<jobject>& java_callback) {
  OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationGuideDecision decision =
      optimization_guide_keyed_service_->CanApplyOptimization(
          *url::GURLAndroid::ToNativeGURL(env, java_gurl),
          static_cast<optimization_guide::proto::OptimizationType>(
              optimization_type),
          &optimization_metadata);
  OnOptimizationGuideDecision(java_callback, decision, optimization_metadata);
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
