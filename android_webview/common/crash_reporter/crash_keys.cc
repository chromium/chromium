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
const char kAppProcessName[] = "app-process-name";

const char kAndroidSdkInt[] = "android-sdk-int";

const char kSupportLibraryWebkitVersion[] = "androidx-webkit-version";

extern const char kWeblayerWebViewCompatMode[] =
    "WEBLAYER_WEB_VIEW_COMPAT_MODE";

// clang-format off
const char* const kWebViewCrashKeyAllowList[] = {
    kAppPackageName,
    kAppPackageVersionCode,
    kAppProcessName,
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
    "gpu-gl-renderer",
    "oop_read_failure",

    // components/viz
    "viz_deserialization",

    // content/:
    "bad_message_reason",
    "discardable-memory-allocated",
    "discardable-memory-free",
    "mojo-message-error",
    "total-discardable-memory-allocated",

    // services/network
    "network_deserialization",

    // GWP-ASan
    gwp_asan::kMallocCrashKey,
    gwp_asan::kPartitionAllocCrashKey,

    // crash keys needed for recording finch trials
    "variations",
    "num-experiments",

    kWeblayerWebViewCompatMode,

    nullptr};
// clang-format on

void InitCrashKeysForWebViewTesting() {
  crash_reporter::InitializeCrashKeys();
}

}  // namespace crash_keys

}  // namespace android_webview
