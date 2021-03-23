// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
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
  DCHECK(!optimization_metadata.public_image_metadata());
  DCHECK(!optimization_metadata.performance_hints_metadata());

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

void OptimizationGuideBridge::CanApplyOptimization(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_gurl,
    jint optimization_type,
    const JavaParamRef<jobject>& java_callback) {
  if (!optimization_guide_keyed_service_->GetHintsManager()) {
    // The decider is not initialized yet, so return unknown.
    OnOptimizationGuideDecision(
        java_callback, optimization_guide::OptimizationGuideDecision::kUnknown,
        {});
    return;
  }

  optimization_guide_keyed_service_->GetHintsManager()
      ->CanApplyOptimizationAsync(
          *url::GURLAndroid::ToNativeGURL(env, java_gurl),
          /*navigation_id=*/base::nullopt,
          static_cast<optimization_guide::proto::OptimizationType>(
              optimization_type),
          base::BindOnce(&OnOptimizationGuideDecision,
                         ScopedJavaGlobalRef<jobject>(env, java_callback)));
}

}  // namespace android
}  // namespace optimization_guide
