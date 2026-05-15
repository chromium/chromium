// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/android/personal_context_first_run_bottom_sheet_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/android/chrome_jni_headers/PersonalContextFirstRunBottomSheetBridge_jni.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace personal_context {

PersonalContextFirstRunBottomSheetBridge::
    PersonalContextFirstRunBottomSheetBridge(
        content::WebContents* web_contents,
        base::OnceCallback<void(accessibility_annotator::InfoResult)> callback)
    : callback_(std::move(callback)) {
  if (!web_contents) {
    return;
  }

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_PersonalContextFirstRunBottomSheetBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
}

PersonalContextFirstRunBottomSheetBridge::
    ~PersonalContextFirstRunBottomSheetBridge() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PersonalContextFirstRunBottomSheetBridge_destroy(env, java_obj_);
  }
}

bool PersonalContextFirstRunBottomSheetBridge::PerformShowContent() {
  if (!java_obj_) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PersonalContextFirstRunBottomSheetBridge_show(
      env, java_obj_,
      accessibility_annotator::kAccessibilityAnnotatorSettingsURL,
      accessibility_annotator::kAccessibilityAnnotatorLearnMoreURL);
}

void PersonalContextFirstRunBottomSheetBridge::Show() {
  if (PerformShowContent()) {
    base::UmaHistogramEnumeration(
        "AccessibilityAnnotator.RemoteAnnotatorInfo",
        accessibility_annotator::InfoShowRequestResult::kShown);
  } else {
    // TODO(crbug.com/502445725): Record "NotShown" histogram value.
    if (callback_) {
      std::move(callback_).Run(
          accessibility_annotator::InfoResult::kNotAcknowledged);
    }
  }
}

void PersonalContextFirstRunBottomSheetBridge::Hide() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PersonalContextFirstRunBottomSheetBridge_hide(env, java_obj_);
  }
}

void PersonalContextFirstRunBottomSheetBridge::OnInfoAcknowledged(JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run(
        accessibility_annotator::InfoResult::kAcknowledged);
    base::UmaHistogramEnumeration(
        "AccessibilityAnnotator.RemoteAnnotatorInfo",
        accessibility_annotator::InfoShowRequestResult::kAccepted);
  }
}

void PersonalContextFirstRunBottomSheetBridge::OnManageSettingsClicked(
    JNIEnv* env) {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));
}

void PersonalContextFirstRunBottomSheetBridge::OnLearnMoreClicked(JNIEnv* env) {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.LearnMoreLinkClick"));
}

void PersonalContextFirstRunBottomSheetBridge::OnInfoDismissed(JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run(
        accessibility_annotator::InfoResult::kNotAcknowledged);
    base::UmaHistogramEnumeration(
        "AccessibilityAnnotator.RemoteAnnotatorInfo",
        accessibility_annotator::InfoShowRequestResult::kDismissed);
  }
}

DEFINE_JNI(PersonalContextFirstRunBottomSheetBridge)

}  // namespace personal_context

using personal_context::PersonalContextFirstRunBottomSheetBridge;
