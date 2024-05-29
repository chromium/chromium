// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "android_webview/common/aw_resource_bundle.h"
#include "android_webview/nonembedded/webview_apk_process.h"
#include "base/android/base_jni_onload.h"
#include "base/check.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/nonembedded/nonembedded_jni_headers/WebViewApkApplication_jni.h"

namespace android_webview {

void JNI_WebViewApkApplication_InitializeGlobalsAndResources(JNIEnv* env) {
  InitIcuAndResourceBundleBrowserSide();
  WebViewApkProcess::Init();
}

}  // namespace android_webview
