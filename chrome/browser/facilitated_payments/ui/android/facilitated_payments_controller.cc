// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsControllerBridge_jni.h"

FacilitatedPaymentsController::FacilitatedPaymentsController() = default;
FacilitatedPaymentsController::~FacilitatedPaymentsController() = default;

bool FacilitatedPaymentsController::Show(
    std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
        view,
    content::WebContents* web_contents) {
  // Abort if facilitated payments surface is already shown.
  if (view_) {
    return false;
  }

  if (!view->RequestShowContent(this, web_contents)) {
    java_object_.Reset();
    return false;
  }

  view_ = std::move(view);
  return true;
}

base::android::ScopedJavaLocalRef<jobject>
FacilitatedPaymentsController::GetJavaObject() {
  if (!java_object_) {
    java_object_ = payments::facilitated::
        Java_FacilitatedPaymentsPaymentMethodsControllerBridge_create(
            base::android::AttachCurrentThread(),
            reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}
