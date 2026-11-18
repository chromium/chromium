// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "third_party/dawn/include/dawn/dawn_version.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwDawnLoader_jni.h"

namespace android_webview {

namespace {
constexpr int64_t kWebGpuErrorAppTooOld = -1;
constexpr int64_t kWebGpuErrorWebViewTooOld = 0;
}  // namespace

// Checks the requested Dawn ABI version and returns the GetStableProcAddress
// function pointer if compatible.
static int64_t JNI_AwDawnLoader_GetWGPUProcGetProcAddress(
    JNIEnv* env,
    int32_t major_version,
    int64_t minor_version) {
  if (major_version < 0 || minor_version < 0 ||
      static_cast<uint32_t>(major_version) < DAWN_MAJOR_VERSION) {
    return kWebGpuErrorAppTooOld;
  }
  if (static_cast<uint32_t>(major_version) > DAWN_MAJOR_VERSION ||
      static_cast<uint64_t>(minor_version) > DAWN_MINOR_VERSION) {
    return kWebGpuErrorWebViewTooOld;
  }
  return static_cast<int64_t>(
      reinterpret_cast<uintptr_t>(&dawn::native::GetStableProcAddress));
}

}  // namespace android_webview

DEFINE_JNI(AwDawnLoader)
