// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge_impl.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/android/pref_service_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/internal/jni/TouchToFillPasswordGenerationBridge_jni.h"

TouchToFillPasswordGenerationBridgeImpl::
    TouchToFillPasswordGenerationBridgeImpl() = default;

TouchToFillPasswordGenerationBridgeImpl::
    ~TouchToFillPasswordGenerationBridgeImpl() = default;

bool TouchToFillPasswordGenerationBridgeImpl::Show(
    content::WebContents* web_contents,
    PrefService* pref_service,
    TouchToFillPasswordGenerationDelegate* delegate,
    std::u16string password,
    std::string account) {
  if (!web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return false;
  }
  delegate_ = delegate;

  CHECK(!java_object_);
  JNIEnv* env = jni_zero::AttachCurrentThread();
  java_object_.Reset(Java_TouchToFillPasswordGenerationBridge_create(
      env, web_contents->GetNativeView()->GetWindowAndroid(), web_contents,
      pref_service, reinterpret_cast<intptr_t>(this)));

  return Java_TouchToFillPasswordGenerationBridge_show(env, java_object_,
                                                       password, account);
}

void TouchToFillPasswordGenerationBridgeImpl::Hide() {
  if (!java_object_) {
    return;
  }

  Java_TouchToFillPasswordGenerationBridge_hideFromNative(
      jni_zero::AttachCurrentThread(), java_object_);
}

void TouchToFillPasswordGenerationBridgeImpl::OnDismissed(
    bool generated_password_accepted) {
  CHECK(delegate_);

  // Calling `delegate_->OnDismissed` will trigger the bridge's destructor,
  // which in its turn will trigger `Hide`. `java_object_` needs to be reset
  // before that, otherwise `Hide` will trigger the second `OnDismissed`
  // call.
  java_object_.Reset();
  delegate_->OnDismissed(generated_password_accepted);
}

void TouchToFillPasswordGenerationBridgeImpl::OnGeneratedPasswordAccepted(
    const std::u16string& password) {
  CHECK(delegate_);
  delegate_->OnGeneratedPasswordAccepted(password);
}

void TouchToFillPasswordGenerationBridgeImpl::OnGeneratedPasswordRejected() {
  CHECK(delegate_);
  delegate_->OnGeneratedPasswordRejected();
}

DEFINE_JNI(TouchToFillPasswordGenerationBridge)
