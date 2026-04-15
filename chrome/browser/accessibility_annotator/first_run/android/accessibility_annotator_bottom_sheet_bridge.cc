// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/android/accessibility_annotator_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/android/chrome_jni_headers/AccessibilityAnnotatorBottomSheetBridge_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace accessibility_annotator {

AccessibilityAnnotatorBottomSheetBridge::
    AccessibilityAnnotatorBottomSheetBridge(
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
  java_obj_.Reset(Java_AccessibilityAnnotatorBottomSheetBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
}

AccessibilityAnnotatorBottomSheetBridge::
    ~AccessibilityAnnotatorBottomSheetBridge() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AccessibilityAnnotatorBottomSheetBridge_destroy(env, java_obj_);
  }
}

bool AccessibilityAnnotatorBottomSheetBridge::PerformShowContent() {
  if (!java_obj_) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AccessibilityAnnotatorBottomSheetBridge_show(env, java_obj_);
}

void AccessibilityAnnotatorBottomSheetBridge::Show() {
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

void AccessibilityAnnotatorBottomSheetBridge::Hide() {
  if (java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AccessibilityAnnotatorBottomSheetBridge_hide(env, java_obj_);
  }
}

void AccessibilityAnnotatorBottomSheetBridge::OnInfoAcknowledged(JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run(InfoResult::kAcknowledged);
    base::UmaHistogramEnumeration("AccessibilityAnnotator.RemoteAnnotatorInfo",
                                  InfoShowRequestResult::kAccepted);
  }
}

void AccessibilityAnnotatorBottomSheetBridge::OnManageSettingsClicked(
    JNIEnv* env) {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));
}

void AccessibilityAnnotatorBottomSheetBridge::OnLearnMoreClicked(JNIEnv* env) {
  base::RecordAction(base::UserMetricsAction(
      "AccessibilityAnnotator.RemoteAnnotatorInfo.LearnMoreLinkClick"));
}

void AccessibilityAnnotatorBottomSheetBridge::OnInfoDismissed(JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run(InfoResult::kNotAcknowledged);
    base::UmaHistogramEnumeration("AccessibilityAnnotator.RemoteAnnotatorInfo",
                                  InfoShowRequestResult::kDismissed);
  }
}

DEFINE_JNI(AccessibilityAnnotatorBottomSheetBridge)

}  // namespace accessibility_annotator

using accessibility_annotator::AccessibilityAnnotatorBottomSheetBridge;
