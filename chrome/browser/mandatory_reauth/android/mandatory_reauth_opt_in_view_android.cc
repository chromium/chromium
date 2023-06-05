// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mandatory_reauth/android/mandatory_reauth_opt_in_view_android.h"

#include <jni.h>

#include "chrome/browser/mandatory_reauth/android/jni_headers/MandatoryReauthOptInBottomSheetViewBridge_jni.h"

namespace autofill {

std::unique_ptr<MandatoryReauthOptInViewAndroid>
MandatoryReauthOptInViewAndroid::CreateAndShow() {
  std::unique_ptr<MandatoryReauthOptInViewAndroid> view =
      std::make_unique<MandatoryReauthOptInViewAndroid>();
  view->Show();
  return view;
}

MandatoryReauthOptInViewAndroid::MandatoryReauthOptInViewAndroid() = default;

MandatoryReauthOptInViewAndroid::~MandatoryReauthOptInViewAndroid() = default;

void MandatoryReauthOptInViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_bridge_.Reset(
      Java_MandatoryReauthOptInBottomSheetViewBridge_create(env));
  CHECK(java_bridge_);
  Java_MandatoryReauthOptInBottomSheetViewBridge_show(env, java_bridge_);
}

void MandatoryReauthOptInViewAndroid::Hide() {
  if (java_bridge_) {
    Java_MandatoryReauthOptInBottomSheetViewBridge_close(
        base::android::AttachCurrentThread(), java_bridge_);
  }
}

}  // namespace autofill
