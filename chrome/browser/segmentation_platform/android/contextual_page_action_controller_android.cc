// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/ContextualPageActionController_jni.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/commerce/core/shopping_service.h"
#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;

void RunGetSelectedSegmentCallback(
    const base::android::JavaRef<jobject>& j_callback,
    const segmentation_platform::SegmentSelectionResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback, segmentation_platform::SegmentationPlatformConversionBridge::
                      CreateJavaSegmentSelectionResult(env, result));
}

static void JNI_ContextualPageActionController_ComputeContextualPageAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_url,
    jboolean j_can_track_price,
    const JavaParamRef<jobject>& j_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile) {
    RunGetSelectedSegmentCallback(
        j_callback, segmentation_platform::SegmentSelectionResult());
    return;
  }
  auto url = url::GURLAndroid::ToNativeGURL(env, j_url);

  scoped_refptr<segmentation_platform::InputContext> input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  input_context->metadata_args.emplace("is_price_tracking",
                                       static_cast<float>(j_can_track_price));
  input_context->metadata_args.emplace("url", *url);
  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  if (!segmentation_platform_service) {
    RunGetSelectedSegmentCallback(
        j_callback, segmentation_platform::SegmentSelectionResult());
    return;
  }
  segmentation_platform_service->GetSelectedSegmentOnDemand(
      segmentation_platform::kContextualPageActionsKey, input_context,
      base::BindOnce(&RunGetSelectedSegmentCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}
