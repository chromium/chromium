// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace android_webview {
namespace features {

// Alphabetical:

// Enables package name logging for the most popular WebView embedders that are
// on a dynamically generated allowlist.
// The filtering for package names will be done on the server side using this
// flag
BASE_FEATURE(kWebViewAppsPackageNamesServerSideAllowlist,
             "WebViewAppsPackageNamesServerSideAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable brotli compression support in WebView.
BASE_FEATURE(kWebViewBrotliSupport,
             "WebViewBrotliSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Check layer_tree_frame_sink_id when return resources to compositor.
BASE_FEATURE(kWebViewCheckReturnResources,
             "WebViewCheckReturnResources",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to destroy the WebView rendering functor when after a WebView window
// becomes invisible.
BASE_FEATURE(kWebViewClearFunctorInBackground,
             "WebViewClearFunctorInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the SafeBrowsingApiHandlerBridge which uses the connectionless GMS APIs.
// This Feature is checked and used in downstream internal code.
BASE_FEATURE(kWebViewConnectionlessSafeBrowsing,
             "WebViewConnectionlessSafeBrowsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for adding CHECKs to loading pak files.
BASE_FEATURE(kWebViewCheckPakFileDescriptors,
             "WebViewCheckPakFileDescriptors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Cache origins which have camera/mic permissions approved to allow subsequent
// calls to enumerate devices to return device labels.
BASE_FEATURE(kWebViewEnumerateDevicesCache,
             "WebViewEnumerateDevicesCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewExitReasonMetric,
             "WebViewExitReasonMetric",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebView to automatically darken the page in FORCE_DARK_AUTO mode if
// the app's theme is dark.
BASE_FEATURE(kWebViewForceDarkModeMatchTheme,
             "WebViewForceDarkModeMatchTheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewHitTestInBlinkOnTouchStart,
             "WebViewHitTestInBlinkOnTouchStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable display cutout support for Android P and above.
BASE_FEATURE(kWebViewDisplayCutout,
             "WebViewDisplayCutout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Fake empty component to measure component updater performance impact on
// WebView clients.
BASE_FEATURE(kWebViewEmptyComponentLoaderPolicy,
             "WebViewEmptyComponentLoaderPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, passive mixed content (Audio/Video/Image subresources loaded
// over HTTP on HTTPS sites) will be autoupgraded to HTTPS, and the load will be
// blocked if the resource fails to load over HTTPS. This only affects apps that
// set the mixed content mode to MIXED_CONTENT_COMPATIBILITY_MODE, autoupgrades
// are always disabled for MIXED_CONTENT_NEVER_ALLOW and
// MIXED_CONTENT_ALWAYS_ALLOW modes.
BASE_FEATURE(kWebViewMixedContentAutoupgrades,
             "WebViewMixedContentAutoupgrades",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Only allow extra headers added via loadUrl() to be sent to the original
// origin; strip them from the request if a cross-origin redirect occurs.
BASE_FEATURE(kWebViewExtraHeadersSameOriginOnly,
             "WebViewExtraHeadersSameOriginOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the new Java/JS Bridge code path with mojo implementation.
BASE_FEATURE(kWebViewJavaJsBridgeMojo,
             "WebViewJavaJsBridgeMojo",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable reporting filtered metrics from webview clients used to be
// out-sampled.
BASE_FEATURE(kWebViewMetricsFiltering,
             "WebViewMetricsFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Field trial feature for controlling support of Origin Trials on WebView.
BASE_FEATURE(kWebViewOriginTrials,
             "WebViewOriginTrials",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to record size of the embedding app's data directory to the UMA
// histogram Android.WebView.AppDataDirectorySize.
BASE_FEATURE(kWebViewRecordAppDataDirectorySize,
             "WebViewRecordAppDataDirectorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to report frame metrics to the Android.Jank.FrameDuration and
// Android.Jank.FrameJankStatus histograms.
BASE_FEATURE(kWebViewReportFrameMetrics,
             "WebViewReportFrameMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Flag to restrict main frame Web Content to verified web content. Verification
// happens via Digital Asset Links.
BASE_FEATURE(kWebViewRestrictSensitiveContent,
             "WebViewRestrictSensitiveContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable doing a JNI call to check safe browsing safe mode status before doing
// a safe browsing check.
BASE_FEATURE(kWebViewSafeBrowsingSafeMode,
             "WebViewSafeBrowsingSafeMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable detection of loading mature sites (according to Google SafeSearch)
// on WebViews running on supervised user accounts.
BASE_FEATURE(kWebViewSupervisedUserSiteDetection,
             "WebViewSupervisedUserSiteDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable blocking the loading of mature sites (according to Google SafeSearch)
// on WebViews running on supervised user accounts.
BASE_FEATURE(kWebViewSupervisedUserSiteBlock,
             "WebViewSupervisedUserSiteBlock",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disallows window.{alert, prompt, confirm} if triggered inside a subframe that
// is not same origin with the main frame.
BASE_FEATURE(kWebViewSuppressDifferentOriginSubframeJSDialogs,
             "WebViewSuppressDifferentOriginSubframeJSDialogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A Feature used for WebView variations tests. Not used in production.
BASE_FEATURE(kWebViewTestFeature,
             "WebViewTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use WebView's nonembedded MetricsUploadService to upload UMA metrics instead
// of sending it directly to GMS-core.
BASE_FEATURE(kWebViewUseMetricsUploadService,
             "WebViewUseMetricsUploadService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Propagate Android's network notification signals to networking stack
BASE_FEATURE(kWebViewPropagateNetworkSignals,
             "webViewPropagateNetworkSignals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable raster in wide color gamut for apps that use webview in a wide color
// gamut activity.
BASE_FEATURE(kWebViewWideColorGamutSupport,
             "WebViewWideColorGamutSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Control the default behaviour for the XRequestedWith header
BASE_FEATURE(kWebViewXRequestedWithHeaderControl,
             "WebViewXRequestedWithHeaderControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default value of the XRequestedWith header mode when
// WebViewXRequestedWithHeaderControl is enabled. Defaults to
// |AwSettings::RequestedWithHeaderMode::NO_HEADER| Must be value declared in in
// |AwSettings::RequestedWithHeaderMode|
const base::FeatureParam<int> kWebViewXRequestedWithHeaderMode{
    &kWebViewXRequestedWithHeaderControl, "WebViewXRequestedWithHeaderMode", 0};

// Control whether WebView will attempt to read the XRW header allow-list from
// the manifest.
BASE_FEATURE(kWebViewXRequestedWithHeaderManifestAllowList,
             "WebViewXRequestedWithHeaderManifestAllowList",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables image drage out for Webview.
BASE_FEATURE(kWebViewImageDrag,
             "WebViewImageDrag",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables uploading UMA data with a higher frequency.
// This Feature is checked and used in downstream internal code.
BASE_FEATURE(kWebViewUmaUploadQualityOfServiceSetToDefault,
             "WebViewUmaUploadQualityOfServiceSetToDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables zoom keyboard shortcuts for zoom-in, zoom-out and zoom reset.
BASE_FEATURE(kWebViewZoomKeyboardShortcuts,
             "WebViewZoomKeyboardShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace android_webview
