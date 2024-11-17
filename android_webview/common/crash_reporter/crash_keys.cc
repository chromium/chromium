// Copyright 2017 The Chromium Authors
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

const char kContextLossReason[] = "context-loss-reason";

const char kSupportLibraryWebkitVersion[] = "androidx-webkit-version";

extern const char kWeblayerWebViewCompatMode[] =
    "WEBLAYER_WEB_VIEW_COMPAT_MODE";

// clang-format off
const char* const kWebViewCrashKeyAllowList[] = {
    kAppPackageName,
    kAppPackageVersionCode,
    kAppProcessName,
    kAndroidSdkInt,
    kContextLossReason,
    kSupportLibraryWebkitVersion,

    // process type
    "ptype",

    // Java exception stack traces
    "exception_info",

    // base
    "base-OpenApkAssetError",

    // gpu
    "gpu-driver",
    "gpu-psver",
    "gpu-vsver",
    "gpu-gl-vendor",
    "gpu-gl-renderer",
    "gr-context-type",
    "oop_read_failure",
    "gpu-gl-error-message",

    // components/viz
    "viz_deserialization",

    // content/:
    "bad_message_reason",
    "can_access_data_failure_reason",
    "cpspi_can_commit_url_failure_reason",
    "discardable-memory-allocated",
    "discardable-memory-free",
    "mojo-message-error",
    "total-discardable-memory-allocated",

    // Navigation
    "ever_had_loaddatawithbaseurl_exemption",
    "ever_had_universal_access_exemption",
    "rfhi_can_commit_failure_reason",
    "is_same_document",
    "is_main_frame",
    "is_opaque_origin",
    "is_file_origin",
    "is_data_url",
    "is_srcdoc_url",
    "is_loaddatawithbaseurl_navrequest",
    "is_loaddatawithbaseurl_samedoc",
    "is_process_locked",
    "is_on_initial_empty_doc",
    "is_renderer_initiated",
    "is_error_page",

    "VerifyDidCommit-prev_ldwb",
    "VerifyDidCommit-prev_ldwbu",
    "VerifyDidCommit-base_url_fdu_type",
    "VerifyDidCommit-data_url_empty",
    "VerifyDidCommit-history_url_fdu_type",

    "VerifyDidCommit-intended_browser",
    "VerifyDidCommit-intended_renderer",

    "VerifyDidCommit-method_post_browser",
    "VerifyDidCommit-method_post_renderer",
    "VerifyDidCommit-original_method_post",

    "VerifyDidCommit-unreachable_browser",
    "VerifyDidCommit-unreachable_renderer",

    "VerifyDidCommit-post_id_matches",
    "VerifyDidCommit-post_id_-1_browser",
    "VerifyDidCommit-post_id_-1_renderer",

    "VerifyDidCommit-override_ua_browser",
    "VerifyDidCommit-override_ua_renderer",

    "VerifyDidCommit-code_browser",
    "VerifyDidCommit-code_renderer",

    "VerifyDidCommit-suh_browser",
    "VerifyDidCommit-suh_renderer",

    "VerifyDidCommit-gesture_browser",
    "VerifyDidCommit-gesture_renderer",

    "VerifyDidCommit-replace_browser",
    "VerifyDidCommit-replace_renderer",

    "VerifyDidCommit-url_relation",
    "VerifyDidCommit-url_browser_type",
    "VerifyDidCommit-url_renderer_type",

    "VerifyDidCommit-is_same_document",
    "VerifyDidCommit-is_history_api",
    "VerifyDidCommit-renderer_initiated",
    "VerifyDidCommit-is_subframe",
    "VerifyDidCommit-is_form_submission",
    "VerifyDidCommit-is_error_page",
    "VerifyDidCommit-net_error",

    "VerifyDidCommit-is_server_redirect",
    "VerifyDidCommit-redirects_size",

    "VerifyDidCommit-entry_offset",
    "VerifyDidCommit-entry_count",
    "VerifyDidCommit-last_committed_index",

    "VerifyDidCommit-is_reload",
    "VerifyDidCommit-is_restore",
    "VerifyDidCommit-is_history",
    "VerifyDidCommit-has_valid_page_state",

    "VerifyDidCommit-has_gesture",
    "VerifyDidCommit-was_click",

    "VerifyDidCommit-original_same_doc",

    "VerifyDidCommit-committed_real_load",
    "VerifyDidCommit-last_url_type",

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
    "variations-seed-version",

    // CRX components
    "crx-components",

    // sandbox/linux
    "seccomp-sigsys",

    kWeblayerWebViewCompatMode,

    // Used to report switches/feature flags overridden in the DevUI
    "commandline-enabled-feature-*",
    "commandline-disabled-feature-*",
    "switch-*",
    "num-switches",

    // NavigationListener investigation
    "NoTrackedNav-message",
    "NoTrackedNav-nav_id",
    "NoTrackedNav-url_type",
    "NoTrackedNav-prev_url_type",

    "NoTrackedNav-discard_reason",
    "NoTrackedNav-tracked_navs_size",
    "NoTrackedNav-all_navs_size",
    "NoTrackedNav-net_error_code",

    "NoTrackedNav-has_committed",
    "NoTrackedNav-was_redirect",
    "NoTrackedNav-is_activation",
    "NoTrackedNav-is_same_doc",
    "NoTrackedNav-is_renderer",
    "NoTrackedNav-is_reload",
    "NoTrackedNav-is_history",
    "NoTrackedNav-is_restore",

    // crbug.com/370872370
    "OriginCalc-debug_info",
    "OriginCalc-url_stripped",
    "OriginCalc-same_ptr",
    "OriginCalc-origin",
    "OriginCalc-origin_to_commit",
    "OriginCalc-origin_local",
    "OriginCalc-origin_to_commit_local",
    "OriginCalc-origin_blcok",
    "OriginCalc-origin_to_commit_block",

    nullptr};
// clang-format on

void InitCrashKeysForWebViewTesting() {
  crash_reporter::InitializeCrashKeys();
}

}  // namespace crash_keys

}  // namespace android_webview
