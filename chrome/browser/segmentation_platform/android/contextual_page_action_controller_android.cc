// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/android/toolbar/adaptive_toolbar_enums.h"
#include "components/segmentation_platform/public/android/input_context_android.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextualPageActionController_jni.h"

using base::android::JavaParamRef;

// TODO(shaktisahu): Split this file to extract a JNI independent class that
// can be unit tested.
namespace {

AdaptiveToolbarButtonVariant ActionLabelToAdaptiveToolbarButtonVariant(
    const std::string& label) {
  AdaptiveToolbarButtonVariant action = AdaptiveToolbarButtonVariant::kUnknown;
  if (label ==
      segmentation_platform::kContextualPageActionModelLabelPriceTracking) {
    action = AdaptiveToolbarButtonVariant::kPriceTracking;
  } else if (label ==
             segmentation_platform::kContextualPageActionModelLabelReaderMode) {
    action = AdaptiveToolbarButtonVariant::kReaderMode;
  } else if (label == segmentation_platform::
                          kContextualPageActionModelLabelPriceInsights) {
    action = AdaptiveToolbarButtonVariant::kPriceInsights;
  } else if (label ==
             segmentation_platform::kContextualPageActionModelLabelDiscounts) {
    action = AdaptiveToolbarButtonVariant::kDiscounts;
  }
  return action;
}

void RunGetClassificationResultCallback(
    const base::android::JavaRef<jobject>& j_callback,
    const segmentation_platform::ClassificationResult& result) {
  std::string action_label =
      result.ordered_labels.empty() ? "" : result.ordered_labels[0];

  base::android::RunIntCallbackAndroid(
      j_callback, static_cast<int32_t>(
                      ActionLabelToAdaptiveToolbarButtonVariant(action_label)));
}

}  // namespace

static void JNI_ContextualPageActionController_ComputeContextualPageAction(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_input_context,
    const JavaParamRef<jobject>& j_callback) {
  if (!profile) {
    RunGetClassificationResultCallback(
        j_callback, segmentation_platform::ClassificationResult(
                        segmentation_platform::PredictionStatus::kFailed));
    return;
  }

  scoped_refptr<segmentation_platform::InputContext> input_context =
      segmentation_platform::InputContextAndroid::ToNativeInputContext(
          env, j_input_context);

  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  if (!segmentation_platform_service) {
    RunGetClassificationResultCallback(
        j_callback, segmentation_platform::ClassificationResult(
                        segmentation_platform::PredictionStatus::kFailed));
    return;
  }

  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  segmentation_platform_service->GetClassificationResult(
      segmentation_platform::kContextualPageActionsKey, prediction_options,
      input_context,
      base::BindOnce(&RunGetClassificationResultCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}
