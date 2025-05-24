// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cct_password_saving_metrics_recorder_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/browser/password_manager/android/jni_headers/CctPasswordSavingMetricsRecorderBridge_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// static
std::unique_ptr<CctPasswordSavingMetricsRecorderBridge>
CctPasswordSavingMetricsRecorderBridge::MaybeCreate(
    content::WebContents* web_contents) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();
  if (window_android == nullptr) {
    return nullptr;
  }
  return base::WrapUnique<CctPasswordSavingMetricsRecorderBridge>(
      new CctPasswordSavingMetricsRecorderBridge(window_android));
}

CctPasswordSavingMetricsRecorderBridge::
    ~CctPasswordSavingMetricsRecorderBridge() {
  CHECK(java_object_);
  Java_CctPasswordSavingMetricsRecorderBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void CctPasswordSavingMetricsRecorderBridge::OnPotentialSaveFormSubmitted() {
  CHECK(java_object_);

  Java_CctPasswordSavingMetricsRecorderBridge_onPotentialSaveFormSubmitted(
      base::android::AttachCurrentThread(), java_object_);
}

CctPasswordSavingMetricsRecorderBridge::CctPasswordSavingMetricsRecorderBridge(
    ui::WindowAndroid* window_android) {
  java_object_ = Java_CctPasswordSavingMetricsRecorderBridge_Constructor(
      base::android::AttachCurrentThread(), window_android->GetJavaObject());
}
