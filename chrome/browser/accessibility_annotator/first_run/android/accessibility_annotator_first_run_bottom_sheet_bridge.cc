// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/android/accessibility_annotator_first_run_bottom_sheet_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/android/chrome_jni_headers/AccessibilityAnnotatorFirstRunBottomSheetBridge_jni.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace accessibility_annotator {

AccessibilityAnnotatorFirstRunBottomSheetBridge::
    AccessibilityAnnotatorFirstRunBottomSheetBridge(
        content::WebContents* web_contents,
        base::OnceCallback<void(InfoResult)> callback)
    : callback_(std::move(callback)) {
  if (!web_contents) {
    return;
  }

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_AccessibilityAnnotatorFirstRunBottomSheetBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
}

AccessibilityAnnotatorFirstRunBottomSheetBridge::
    ~AccessibilityAnnotatorFirstRunBottomSheetBridge() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AccessibilityAnnotatorFirstRunBottomSheetBridge_destroy(env,
                                                                 java_obj_);
  }
}

bool AccessibilityAnnotatorFirstRunBottomSheetBridge::PerformShowContent() {
  if (!java_obj_) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AccessibilityAnnotatorFirstRunBottomSheetBridge_show(
      env, java_obj_, kAccessibilityAnnotatorSettingsURL,
      kAccessibilityAnnotatorLearnMoreURL);
}

void AccessibilityAnnotatorFirstRunBottomSheetBridge::Show() {
  if (PerformShowContent()) {
    base::UmaHistogramEnumeration("AccessibilityAnnotator.RemoteAnnotatorInfo",
                                  InfoShowRequestResult::kShown);
  } else {
    // TODO(crbug.com/502445725): Record "NotShown" histogram value.
    if (callback_) {
      std::move(callback_).Run(InfoResult::kNotAcknowledged);
    }
  }
}

void AccessibilityAnnotatorFirstRunBottomSheetBridge::Hide() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AccessibilityAnnotatorFirstRunBottomSheetBridge_hide(env, java_obj_);
  }
}

void AccessibilityAnnotatorFirstRunBottomSheetBridge::OnInfoAcknowledged(
    JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run(InfoResult::kAcknowledged);
    base::UmaHistogramEnumeration("AccessibilityAnnotator.RemoteAnnotatorInfo",
                                  InfoShowRequestResult::kAccepted);
  }
}

void AccessibilityAnnotatorFirstRunBottomSheetBridge::OnManageSettingsClicked(
    JNIEnv* env) {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));
}

void AccessibilityAnnotatorFirstRunBottomSheetBridge::OnLearnMoreClicked(
    JNIEnv* env) {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.LearnMoreLinkClick"));
}

void AccessibilityAnnotatorFirstRunBottomSheetBridge::OnInfoDismissed(
    JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run(InfoResult::kNotAcknowledged);
    base::UmaHistogramEnumeration("AccessibilityAnnotator.RemoteAnnotatorInfo",
                                  InfoShowRequestResult::kDismissed);
  }
}

DEFINE_JNI(AccessibilityAnnotatorFirstRunBottomSheetBridge)

}  // namespace accessibility_annotator

using accessibility_annotator::AccessibilityAnnotatorFirstRunBottomSheetBridge;
