// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_DESCRIPTORS_H_
#define ANDROID_WEBVIEW_COMMON_AW_DESCRIPTORS_H_

#include "content/public/common/content_descriptors.h"

enum {
  kAndroidWebViewLocalePakDescriptor = kContentIPCDescriptorMax + 1,
  kAndroidWebViewMainPakDescriptor,
  kAndroidWebView100PercentPakDescriptor,
  kAndroidWebViewCrashSignalDescriptor,
  kAndroidMinidumpDescriptor,
};

#endif  // ANDROID_WEBVIEW_COMMON_AW_DESCRIPTORS_H_
