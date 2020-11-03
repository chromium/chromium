// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * Constants for the names of WebView Features.
 *
 * These must match the names in aw_features.cc.
 */
public final class AwFeatures {
    // Alphabetical:
    public static final String WEBVIEW_CONNECTIONLESS_SAFE_BROWSING =
            "WebViewConnectionlessSafeBrowsing";

    public static final String WEBVIEW_CPU_AFFINITY_RESTRICT_TO_LITTLE_CORES =
            "WebViewCpuAffinityRestrictToLittleCores";

    public static final String WEBVIEW_DISPLAY_CUTOUT = "WebViewDisplayCutout";

    public static final String WEBVIEW_EXTRA_HEADERS_SAME_DOMAIN_ONLY =
            "WebViewExtraHeadersSameDomainOnly";

    public static final String WEBVIEW_EXTRA_HEADERS_SAME_ORIGIN_ONLY =
            "WebViewExtraHeadersSameOriginOnly";

    public static final String WEBVIEW_TEST_FEATURE = "WebViewTestFeature";

    private AwFeatures() {}
}
