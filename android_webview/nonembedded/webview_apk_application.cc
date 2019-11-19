// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/nonembedded_jni_headers/WebViewApkApplication_jni.h"

#include "android_webview/common/aw_resource_bundle.h"
#include "base/android/base_jni_onload.h"
#include "base/logging.h"

namespace android_webview {

void JNI_WebViewApkApplication_InitializePakResources(JNIEnv* env) {
  CHECK(base::android::OnJNIOnLoadInit());
  InitIcuAndResourceBundleBrowserSide();
}

}  // namespace android_webview
