// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_bridge_impl.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/internal/jni/TouchToFillOneTimeTokensBridge_jni.h"
#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

TouchToFillOneTimeTokensBridgeImpl::TouchToFillOneTimeTokensBridgeImpl() =
    default;

TouchToFillOneTimeTokensBridgeImpl::~TouchToFillOneTimeTokensBridgeImpl() =
    default;

bool TouchToFillOneTimeTokensBridgeImpl::Show(
    content::WebContents* web_contents,
    TouchToFillOneTimeTokensDelegate* delegate,
    const std::u16string& token) {
  if (!web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return false;
  }
  delegate_ = delegate;
  JNIEnv* env = base::android::AttachCurrentThread();

  java_bridge_.Reset(Java_TouchToFillOneTimeTokensBridge_create(
      env, web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
      reinterpret_cast<intptr_t>(this)));

  base::android::ScopedJavaLocalRef<jstring> j_token =
      base::android::ConvertUTF16ToJavaString(env, token);

  return Java_TouchToFillOneTimeTokensBridge_show(env, java_bridge_, j_token);
}

void TouchToFillOneTimeTokensBridgeImpl::Hide() {
  if (!java_bridge_) {
    return;
  }

  Java_TouchToFillOneTimeTokensBridge_hide(base::android::AttachCurrentThread(),
                                           java_bridge_);
}

void TouchToFillOneTimeTokensBridgeImpl::OnDismissed(JNIEnv* env,
                                                     bool token_accepted) {
  CHECK(delegate_);
  delegate_->OnDismissed(token_accepted);
}

void TouchToFillOneTimeTokensBridgeImpl::OnTokenAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& token) {
  CHECK(delegate_);
  delegate_->OnTokenAccepted(
      base::android::ConvertJavaStringToUTF16(env, token));
}

void TouchToFillOneTimeTokensBridgeImpl::OnTokenRejected(JNIEnv* env) {
  CHECK(delegate_);
  delegate_->OnTokenRejected();
}
