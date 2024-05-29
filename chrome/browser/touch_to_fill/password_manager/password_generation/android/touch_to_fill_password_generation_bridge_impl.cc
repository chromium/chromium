// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge_impl.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
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
  java_object_.Reset(Java_TouchToFillPasswordGenerationBridge_create(
      base::android::AttachCurrentThread(),
      web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
      web_contents->GetJavaWebContents(), pref_service->GetJavaObject(),
      reinterpret_cast<intptr_t>(this)));

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_password =
      base::android::ConvertUTF16ToJavaString(env, password);
  base::android::ScopedJavaLocalRef<jstring> j_account =
      base::android::ConvertUTF8ToJavaString(env, account);

  return Java_TouchToFillPasswordGenerationBridge_show(env, java_object_,
                                                       j_password, j_account);
}

void TouchToFillPasswordGenerationBridgeImpl::Hide() {
  if (!java_object_) {
    return;
  }

  Java_TouchToFillPasswordGenerationBridge_hideFromNative(
      base::android::AttachCurrentThread(), java_object_);
}

void TouchToFillPasswordGenerationBridgeImpl::OnDismissed(
    JNIEnv* env,
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
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& password) {
  CHECK(delegate_);
  delegate_->OnGeneratedPasswordAccepted(
      base::android::ConvertJavaStringToUTF16(env, password));
}

void TouchToFillPasswordGenerationBridgeImpl::OnGeneratedPasswordRejected(
    JNIEnv* env) {
  CHECK(delegate_);
  delegate_->OnGeneratedPasswordRejected();
}
