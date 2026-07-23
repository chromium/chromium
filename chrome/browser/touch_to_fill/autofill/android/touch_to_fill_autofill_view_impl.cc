// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_view_impl.h"

#include <cstdint>

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/TouchToFillAutofillViewBridge_jni.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace autofill {

TouchToFillAutofillViewImpl::TouchToFillAutofillViewImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

TouchToFillAutofillViewImpl::~TouchToFillAutofillViewImpl() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_TouchToFillAutofillViewBridge_destroy(env, java_object_);
  }
}

bool TouchToFillAutofillViewImpl::ShowPersonalContextNotice(
    TouchToFillAutofillController* controller) {
  if (!web_contents_ || !web_contents_->GetTopLevelNativeWindow()) {
    return false;
  }
  controller_ = controller;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_) {
    java_object_ = Java_TouchToFillAutofillViewBridge_create(
        env, reinterpret_cast<intptr_t>(this),
        web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
        web_contents_->GetJavaWebContents());
    if (!java_object_) {
      return false;
    }
  }
  Java_TouchToFillAutofillViewBridge_show(env, java_object_);
  return true;
}

void TouchToFillAutofillViewImpl::Hide() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_TouchToFillAutofillViewBridge_hide(env, java_object_);
  }
}

void TouchToFillAutofillViewImpl::OnNoticeAcknowledged(JNIEnv* env) {
  if (controller_) {
    controller_->OnNoticeAcknowledged();
  }
}

void TouchToFillAutofillViewImpl::OnSettingsLinkClicked(JNIEnv* env) {
  if (controller_) {
    controller_->OnSettingsLinkClicked();
  }
}

void TouchToFillAutofillViewImpl::OnDismissed(JNIEnv* env) {
  if (controller_) {
    controller_->OnDismissed();
  }
}

}  // namespace autofill

DEFINE_JNI(TouchToFillAutofillViewBridge)
