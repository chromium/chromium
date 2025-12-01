// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/gfx/aw_gr_context_options_provider.h"

#include "android_webview/common/aw_features.h"
#include "android_webview/common_jni/AwGrContextOptionsProvider_jni.h"

namespace android_webview {

void AwGrContextOptionsProvider::SetCustomGrContextOptions(
    GrContextOptions& options) const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (Java_AwGrContextOptionsProvider_shouldEnableTvSmoothing(env)) {
    // crbug.com/364872963
    options.fInternalMultisampleCount = 0;
    options.fSharpenMipmappedTextures = false;
  }
}

}  // namespace android_webview

DEFINE_JNI(AwGrContextOptionsProvider)
