// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsViewBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace payments::facilitated {

FacilitatedPaymentsBottomSheetBridge::FacilitatedPaymentsBottomSheetBridge()
    : java_bridge_(Java_FacilitatedPaymentsPaymentMethodsViewBridge_Constructor(
          base::android::AttachCurrentThread())) {}

FacilitatedPaymentsBottomSheetBridge::~FacilitatedPaymentsBottomSheetBridge() =
    default;

bool FacilitatedPaymentsBottomSheetBridge::RequestShowContent(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents->GetJavaWebContents();
  if (!java_web_contents) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_FacilitatedPaymentsPaymentMethodsViewBridge_requestShowContent(
      env, java_bridge_, java_web_contents);
}

}  // namespace payments::facilitated
