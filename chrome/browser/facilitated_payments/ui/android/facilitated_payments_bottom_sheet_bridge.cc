// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsViewBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace payments::facilitated {

FacilitatedPaymentsBottomSheetBridge::FacilitatedPaymentsBottomSheetBridge() =
    default;

FacilitatedPaymentsBottomSheetBridge::~FacilitatedPaymentsBottomSheetBridge() =
    default;

bool FacilitatedPaymentsBottomSheetBridge::RequestShowContent(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  if (!web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return false;  // No window attached (yet or anymore).
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  java_bridge_.Reset(Java_FacilitatedPaymentsPaymentMethodsViewBridge_create(
      env, web_contents->GetTopLevelNativeWindow()->GetJavaObject()));
  if (!java_bridge_) {
    return false;
  }

  Java_FacilitatedPaymentsPaymentMethodsViewBridge_requestShowContent(
      env, java_bridge_);
  return true;
}

}  // namespace payments::facilitated
