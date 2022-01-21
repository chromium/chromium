// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_features.h"
#include "base/feature_list.h"

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

// Enable WebView to set prefers-color-theme according to the app's theme unless
// app specifies FORCE_DARK_OFF or DARK_STRATEGY_USER_AGENT_DARKENING_ONLY.
const base::Feature kWebViewDarkModeMatchTheme{
    "WebViewDarkModeMatchTheme", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebView to automatically darken the page in FORCE_DARK_AUTO mode if
// the app's theme is dark.
const base::Feature kWebViewForceDarkModeMatchTheme{
    "WebViewForceDarkModeMatchTheme", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable display cutout support for Android P and above.
const base::Feature kWebViewDisplayCutout{"WebViewDisplayCutout",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Fake empty component to measure component updater performance impact on
// WebView clients.
const base::Feature kWebViewEmptyComponent{"WebViewEmptyComponentLoaderPolicy",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enable the new Java/JS Bridge code path with mojo implementation.
const base::Feature kWebViewJavaJsBridgeMojo{"WebViewJavaJsBridgeMojo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, connections using legacy TLS 1.0/1.1 versions are allowed.
const base::Feature kWebViewLegacyTlsSupport{"WebViewLegacyTlsSupport",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables logging whether it was a first party page when logging PageTimeSpent.
const base::Feature kWebViewLogFirstPartyPageTimeSpent{
    "WebViewLogFirstPartyPageTimeSpent", base::FEATURE_DISABLED_BY_DEFAULT};

// Measure the number of pixels occupied by one or more WebViews as a
// proportion of the total screen size. Depending on the number of
// WebVieaws and the size of the screen this might be expensive so
// hidden behind a feature flag until the true runtime cost can be
// measured.
const base::Feature kWebViewMeasureScreenCoverage{
    "WebViewMeasureScreenCoverage", base::FEATURE_DISABLED_BY_DEFAULT};

// Field trial feature for controlling support of Origin Trials on WebView.
const base::Feature kWebViewOriginTrials{"WebViewOriginTrials",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables package name logging for the most popular WebView embedders that are
// on a dynamically generated allowlist.
const base::Feature kWebViewAppsPackageNamesAllowlist{
    "WebViewAppsPackageNamesAllowlist", base::FEATURE_DISABLED_BY_DEFAULT};

// Disallows window.{alert, prompt, confirm} if triggered inside a subframe that
// is not same origin with the main frame.
const base::Feature kWebViewSuppressDifferentOriginSubframeJSDialogs{
    "WebViewSuppressDifferentOriginSubframeJSDialogs",
    base::FEATURE_DISABLED_BY_DEFAULT};

// A Feature used for WebView variations tests. Not used in production.
const base::Feature kWebViewTestFeature{"WebViewTestFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Use WebView's nonembedded MetricsUploadService to upload UMA metrics instead
// of sending it directly to GMS-core.
const base::Feature kWebViewUseMetricsUploadService{
    "WebViewUseMetricsUploadService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable raster in wide color gamut for apps that use webview in a wide color
// gamut activity.
const base::Feature kWebViewWideColorGamutSupport{
    "WebViewWideColorGamutSupport", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace android_webview
