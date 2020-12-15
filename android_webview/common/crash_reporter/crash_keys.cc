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

    // Navigation
    "VerifyDidCommit-browser_intended",
    "VerifyDidCommit-renderer_intended",

    "VerifyDidCommit-browser_method",
    "VerifyDidCommit-renderer_method",
    "VerifyDidCommit-original_method",

    "VerifyDidCommit-browser_unreachable",
    "VerifyDidCommit-renderer_unreachable",

    "VerifyDidCommit-base_url_exp_match",
    "VerifyDidCommit-prev_ldwb",
    "VerifyDidCommit-prev_ldwbu",
    "VerifyDidCommit-b_base_url_valid",
    "VerifyDidCommit-b_base_url_empty",
    "VerifyDidCommit-b_hist_url_empty",
    "VerifyDidCommit-b_data_url_empty",
    "VerifyDidCommit-r_base_url_empty",
    "VerifyDidCommit-r_base_url_error",
    "VerifyDidCommit-r_history_url_empty",

    "VerifyDidCommit-browser_post_id",
    "VerifyDidCommit-renderer_post_id",

    "VerifyDidCommit-browser_override_ua",
    "VerifyDidCommit-renderer_override_ua",

    "VerifyDidCommit-browser_code",
    "VerifyDidCommit-renderer_code",

    "VerifyDidCommit-browser_suh",
    "VerifyDidCommit-renderer_suh",

    "VerifyDidCommit-is_same_document",
    "VerifyDidCommit-is_history_api",
    "VerifyDidCommit-renderer_initiated",
    "VerifyDidCommit-is_subframe",
    "VerifyDidCommit-is_form_submission",
    "VerifyDidCommit-net_error",

    "VerifyDidCommit-is_server_redirect",
    "VerifyDidCommit-redirects_size",

    "VerifyDidCommit-entry_offset",
    "VerifyDidCommit-is_reload",
    "VerifyDidCommit-is_restore",
    "VerifyDidCommit-has_gesture",
    "VerifyDidCommit-was_click",

    "VerifyDidCommit-nav_url_blank",
    "VerifyDidCommit-nav_url_srcdoc",
    "VerifyDidCommit-nav_url_blocked",
    "VerifyDidCommit-nav_url_error",

    "VerifyDidCommit-original_same_doc",

    "VerifyDidCommit-last_url_empty",
    "VerifyDidCommit-last_url_blank",
    "VerifyDidCommit-last_url_srcdoc",
    "VerifyDidCommit-last_url_error",

    "VerifyDidCommit-last_method",
    "VerifyDidCommit-last_code",

    "VerifyDidCommit-has_si_url",

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
