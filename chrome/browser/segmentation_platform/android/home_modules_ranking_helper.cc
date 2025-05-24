// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/android/input_context_android.h"
#include "components/segmentation_platform/public/android/prediction_options_android.h"
#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/segmentation_platform/client_util_jni_headers/HomeModulesRankingHelper_jni.h"

using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;
using ::jni_zero::JavaRef;

namespace {

void RunGetClassificationResultCallback(
    const JavaRef<jobject>& j_callback,
    const segmentation_platform::ClassificationResult& result) {
  JNIEnv* env = AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback, segmentation_platform::SegmentationPlatformConversionBridge::
                      CreateJavaClassificationResult(env, result));
}

}  // namespace

static void JNI_HomeModulesRankingHelper_GetClassificationResult(
    JNIEnv* env,
    Profile* profile,
    const jni_zero::JavaParamRef<jobject>& prediction_options,
    const jni_zero::JavaParamRef<jobject>& input_context,
    const jni_zero::JavaParamRef<jobject>& callback) {
  segmentation_platform::SegmentationPlatformService* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile);
  segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetHomeModulesCardRegistry(profile);
  scoped_refptr<segmentation_platform::InputContext> native_input_context =
      segmentation_platform::InputContextAndroid::ToNativeInputContext(
          env, input_context);

  segmentation_platform::PredictionOptions native_prediction_options =
      segmentation_platform::PredictionOptionsAndroid::
          ToNativePredictionOptions(env, prediction_options);
  registry->get_rank_fecther_helper()->GetHomeModulesRank(
      service, native_prediction_options, native_input_context,
      base::BindOnce(&RunGetClassificationResultCallback,
                     ScopedJavaGlobalRef<jobject>(callback)));
}

static void JNI_HomeModulesRankingHelper_NotifyCardShown(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jstring>& card_label) {
  DCHECK(profile);
  segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetHomeModulesCardRegistry(profile);
  std::string native_card_label = ConvertJavaStringToUTF8(env, card_label);
  registry->NotifyCardShown(native_card_label.c_str());
}

static void JNI_HomeModulesRankingHelper_NotifyCardInteracted(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jstring>& card_label) {
  DCHECK(profile);
  segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetHomeModulesCardRegistry(profile);
  std::string native_card_label = ConvertJavaStringToUTF8(env, card_label);
  registry->NotifyCardInteracted(native_card_label.c_str());
}
