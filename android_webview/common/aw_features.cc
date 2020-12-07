// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_features.h"

namespace android_webview {
namespace features {

// Alphabetical:

// Enable brotli compression support in WebView.
const base::Feature kWebViewBrotliSupport{"WebViewBrotliSupport",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SafeBrowsingApiHandler which uses the connectionless GMS APIs. This
// Feature is checked and used in downstream internal code.
const base::Feature kWebViewConnectionlessSafeBrowsing{
    "WebViewConnectionlessSafeBrowsing", base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts WebView child processes to use only LITTLE cores on big.LITTLE
// architectures.
const base::Feature kWebViewCpuAffinityRestrictToLittleCores{
    "WebViewCpuAffinityRestrictToLittleCores",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable display cutout support for Android P and above.
const base::Feature kWebViewDisplayCutout{"WebViewDisplayCutout",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, passive mixed content (Audio/Video/Image subresources loaded
// over HTTP on HTTPS sites) will be autoupgraded to HTTPS, and the load will be
// blocked if the resource fails to load over HTTPS. This only affects apps that
// set the mixed content mode to MIXED_CONTENT_COMPATIBILITY_MODE, autoupgrades
// are always disabled for MIXED_CONTENT_NEVER_ALLOW and
// MIXED_CONTENT_ALWAYS_ALLOW modes.
const base::Feature kWebViewMixedContentAutoupgrades{
    "WebViewMixedContentAutoupgrades", base::FEATURE_DISABLED_BY_DEFAULT};

// Only allow extra headers added via loadUrl() to be sent to the original
// origin; strip them from the request if a cross-origin redirect occurs.
const base::Feature kWebViewExtraHeadersSameOriginOnly{
    "WebViewExtraHeadersSameOriginOnly", base::FEATURE_DISABLED_BY_DEFAULT};

// Measure the number of pixels occupied by one or more WebViews as a
// proportion of the total screen size. Depending on the number of
// WebVieaws and the size of the screen this might be expensive so
// hidden behind a feature flag until the true runtime cost can be
// measured.
const base::Feature kWebViewMeasureScreenCoverage{
    "WebViewMeasureScreenCoverage", base::FEATURE_DISABLED_BY_DEFAULT};

// A Feature used for WebView variations tests. Not used in production.
const base::Feature kWebViewTestFeature{"WebViewTestFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable raster in wide color gamut for apps that use webview in a wide color
// gamut activity.
const base::Feature kWebViewWideColorGamutSupport{
    "WebViewWideColorGamutSupport", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace android_webview
