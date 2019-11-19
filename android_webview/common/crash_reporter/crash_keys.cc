// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/crash_reporter/crash_keys.h"

#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/common/crash_key_name.h"

namespace android_webview {
namespace crash_keys {

const char kAppPackageName[] = "app-package-name";
const char kAppPackageVersionCode[] = "app-package-version-code";

const char kAndroidSdkInt[] = "android-sdk-int";

const char kSupportLibraryWebkitVersion[] = "androidx-webkit-version";

// clang-format off
const char* const kWebViewCrashKeyWhiteList[] = {
    "AW_WHITELISTED_DEBUG_KEY",
    kAppPackageName,
    kAppPackageVersionCode,
    kAndroidSdkInt,
    kSupportLibraryWebkitVersion,

    // process type
    "ptype",

    // Java exception stack traces
    "exception_info",

    // gpu
    "gpu-driver",
    "gpu-psver",
    "gpu-vsver",
    "gpu-gl-vendor",
    "gpu-gl-vendor__1",
    "gpu-gl-vendor__2",
    "gpu-gl-renderer",
    "oop_read_failure",

    // content/:
    "bad_message_reason",
    "discardable-memory-allocated",
    "discardable-memory-free",
    "mojo-message-error__1",
    "mojo-message-error__2",
    "mojo-message-error__3",
    "mojo-message-error__4",
    "total-discardable-memory-allocated",

    // GWP-ASan
    gwp_asan::kMallocCrashKey,
    gwp_asan::kPartitionAllocCrashKey,

    // crash keys needed for recording finch trials
    "variations",
    "variations__1",
    "variations__2",
    "variations__3",
    "variations__4",
    "variations__5",
    "variations__6",
    "variations__7",
    "variations__8",
    "num-experiments",
    nullptr};
// clang-format on

void InitCrashKeysForWebViewTesting() {
  crash_reporter::InitializeCrashKeys();
}

}  // namespace crash_keys

}  // namespace android_webview
