// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_view_impl.h"

#include "base/android/jni_android.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_controller.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

TouchToFillAutofillViewImpl::TouchToFillAutofillViewImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

TouchToFillAutofillViewImpl::~TouchToFillAutofillViewImpl() = default;

bool TouchToFillAutofillViewImpl::ShowPersonalContextNotice(
    TouchToFillAutofillController* controller) {
  // TODO(crbug.com/521715456): Implement show notice when Java bridge is
  // implemented.
  return true;
}

void TouchToFillAutofillViewImpl::Hide() {
  // TODO(crbug.com/521715456): Implement hide notice when Java bridge is
  // implemented.
}

void TouchToFillAutofillViewImpl::OnNoticeAcknowledged(JNIEnv* env) {
  // TODO(crbug.com/521715456): Implement method when Java bridge is
  // implemented.
}

void TouchToFillAutofillViewImpl::OnDismissed(JNIEnv* env) {
  // TODO(crbug.com/521715456): Implement method when Java bridge is
  // implemented.
}

}  // namespace autofill
