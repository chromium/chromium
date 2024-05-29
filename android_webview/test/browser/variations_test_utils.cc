// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/webview_instrumentation_test_native_jni/VariationsTestUtils_jni.h"

#include "android_webview/browser/aw_feature_list_creator.h"

namespace android_webview {

void JNI_VariationsTestUtils_DisableSignatureVerificationForTesting(
    JNIEnv* env) {
  AwFeatureListCreator::DisableSignatureVerificationForTesting();
}

}  // namespace android_webview
