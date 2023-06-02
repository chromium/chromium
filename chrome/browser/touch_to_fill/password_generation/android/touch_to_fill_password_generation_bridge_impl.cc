// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge_impl.h"

#include "base/check.h"
#include "chrome/browser/touch_to_fill/password_generation/android/jni_headers/TouchToFillPasswordGenerationBridge_jni.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

TouchToFillPasswordGenerationBridgeImpl::
    TouchToFillPasswordGenerationBridgeImpl() {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordGenerationBottomSheet));
}

TouchToFillPasswordGenerationBridgeImpl::
    ~TouchToFillPasswordGenerationBridgeImpl() = default;

bool TouchToFillPasswordGenerationBridgeImpl::Show(
    content::WebContents* web_contents,
    base::WeakPtr<TouchToFillPasswordGenerationDelegate> delegate) {
  if (!web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return false;
  }
  delegate_ = delegate;

  CHECK(!java_object_);
  java_object_.Reset(Java_TouchToFillPasswordGenerationBridge_create(
      base::android::AttachCurrentThread(),
      web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
      reinterpret_cast<intptr_t>(this)));

  return Java_TouchToFillPasswordGenerationBridge_show(
      base::android::AttachCurrentThread(), java_object_);
}

void TouchToFillPasswordGenerationBridgeImpl::Hide() {
  if (!java_object_) {
    return;
  }

  Java_TouchToFillPasswordGenerationBridge_hide(
      base::android::AttachCurrentThread(), java_object_);
}

void TouchToFillPasswordGenerationBridgeImpl::OnDismissed(JNIEnv* env) {
  CHECK(delegate_);
  delegate_->OnDismissed();
}
