// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mandatory_reauth/android/mandatory_reauth_opt_in_view_android.h"

#include <jni.h>

#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/mandatory_reauth/android/internal/jni/MandatoryReauthOptInBottomSheetViewBridge_jni.h"

namespace autofill {

std::unique_ptr<MandatoryReauthOptInViewAndroid>
MandatoryReauthOptInViewAndroid::CreateAndShow(
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller) {
  std::unique_ptr<MandatoryReauthOptInViewAndroid> view =
      std::make_unique<MandatoryReauthOptInViewAndroid>();
  if (view->Show(web_contents, controller)) {
    return view;
  }

  return nullptr;
}

MandatoryReauthOptInViewAndroid::MandatoryReauthOptInViewAndroid() = default;

MandatoryReauthOptInViewAndroid::~MandatoryReauthOptInViewAndroid() = default;

bool MandatoryReauthOptInViewAndroid::Show(
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  if (!view_android) {
    return false;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return false;
  }

  java_view_bridge_.Reset(Java_MandatoryReauthOptInBottomSheetViewBridge_create(
      env, window_android->GetJavaObject(),
      controller->GetJavaControllerBridge()));
  if (!java_view_bridge_) {
    return false;
  }

  return Java_MandatoryReauthOptInBottomSheetViewBridge_show(env,
                                                             java_view_bridge_);
}

void MandatoryReauthOptInViewAndroid::Hide() {
  if (java_view_bridge_) {
    Java_MandatoryReauthOptInBottomSheetViewBridge_close(
        base::android::AttachCurrentThread(), java_view_bridge_);
  }
}

}  // namespace autofill
