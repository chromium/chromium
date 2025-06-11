// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "services/network/public/cpp/features.h"

namespace android_webview::features {

// Alphabetical:

// Enable auto granting storage access API requests. This will be done
// if a relationship is detected between the app and the website.
BASE_FEATURE(kWebViewAutoSAA,
             "WebViewAutoSAA",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable back/forward cache support in WebView. Note that this will only take
// effect iff both this feature flag and the content/public kBackForwardCache
// flag is enabled.
BASE_FEATURE(kWebViewBackForwardCache,
             "WebViewBackForwardCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable loading include statements when checking digital asset links
BASE_FEATURE(kWebViewDigitalAssetLinksLoadIncludes,
             "WebViewDigitalAssetLinksLoadIncludes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables partitioned cookies by default on WebView. This can still be
// overridden by our `setPartitionedCookiesEnabled` Android X API.
BASE_FEATURE(kWebViewDisableCHIPS,
             "WebViewDisableCHIPS",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables MSAA and default sharpening when rendering scaled elements. This is
// often preferable when rendering images/video but can have adverse effects for
// text on some displays.
BASE_FEATURE(kWebViewDisableSharpeningAndMSAA,
             "WebViewDisableSharpeningAndMSAA",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables draining the WebView prefetch queue (for prefetches triggered from
// background thread) during WebView instance initialization and before
// WebView#loadUrl().
// TODO(crbug.com/419251646): remove for M139.
BASE_FEATURE(kWebViewDrainPrefetchQueueDuringInit,
             "WebViewDrainPrefetchQueueDuringInit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable JS FileSystemAccess API.
// This flag is set by WebView internal code based on an app's targetSdkVersion.
// It is enabled for version B+. The default value here is not relevant, and is
// not expected to be manually changed.
// TODO(b/364980165): Flag can be removed when SDK versions prior to B are no
// longer supported.
BASE_FEATURE(kWebViewFileSystemAccess,
             "WebViewFileSystemAccess",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature parameter for `network::features::kMaskedDomainList` that sets the
// exclusion criteria for defining which domains are excluded from the
// Masked Domain List for WebView.
//
// Exclusion criteria can assume values from `WebviewExclusionPolicy`.
const base::FeatureParam<int> kWebViewIpProtectionExclusionCriteria{
    &network::features::kMaskedDomainList,
    "WebViewIpProtectionExclusionCriteria",
    /*WebviewExclusionPolicy::kNone*/ 0};

// Fetch Hand Writing icon lazily.
BASE_FEATURE(kWebViewLazyFetchHandWritingIcon,
             "WebViewLazyFetchHandWritingIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the WebView Media Integrity API as a Blink extension.
// This feature requires `kWebViewMediaIntegrityApi` to be disabled.
BASE_FEATURE(kWebViewMediaIntegrityApiBlinkExtension,
             "WebViewMediaIntegrityApiBlinkExtension",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, passive mixed content (Audio/Video/Image subresources loaded
// over HTTP on HTTPS sites) will be autoupgraded to HTTPS, and the load will be
// blocked if the resource fails to load over HTTPS. This only affects apps that
// set the mixed content mode to MIXED_CONTENT_COMPATIBILITY_MODE, autoupgrades
// are always disabled for MIXED_CONTENT_NEVER_ALLOW and
// MIXED_CONTENT_ALWAYS_ALLOW modes.
BASE_FEATURE(kWebViewMixedContentAutoupgrades,
             "WebViewMixedContentAutoupgrades",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables WebView audio to be muted using an API.
BASE_FEATURE(kWebViewMuteAudio,
             "WebViewMuteAudio",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to record size of the embedding app's data directory to the UMA
// histogram Android.WebView.AppDataDirectorySize.
BASE_FEATURE(kWebViewRecordAppDataDirectorySize,
             "WebViewRecordAppDataDirectorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A Feature used for WebView variations tests. Not used in production. Please
// do not clean up this stale feature: we intentionally keep this feature flag
// around for testing purposes.
BASE_FEATURE(kWebViewTestFeature,
             "WebViewTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use WebView's nonembedded MetricsUploadService to upload UMA metrics instead
// of sending it directly to GMS-core.
BASE_FEATURE(kWebViewUseMetricsUploadService,
             "WebViewUseMetricsUploadService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use WebView's nonembedded MetricsUploadService to upload UMA metrics instead
// of sending it directly to GMS-core when running within the SDK Runtime.
BASE_FEATURE(kWebViewUseMetricsUploadServiceOnlySdkRuntime,
             "WebViewUseMetricsUploadServiceOnlySdkRuntime",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Propagate Android's network change notification signals to the networking
// stack. This only propagates the following notifications:
// * OnNetworkConnected
// * OnNetworkDisconnected
// * OnNetworkMadeDefault
// * OnNetworkSoonToDisconnect.
// AreNetworkHandlesCurrentlySupported is also controlled through this flag.
BASE_FEATURE(kWebViewPropagateNetworkChangeSignals,
             "webViewPropagateNetworkChangeSignals",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Provide the unreduced product version from the AwContentBrowserClient API,
// regardless of the user agent reduction policy.
BASE_FEATURE(kWebViewUnreducedProductVersion,
             "WebViewUnreducedProductVersion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Control the default behaviour for the XRequestedWith header.
// TODO(crbug.com/40286009): enable by default after M120 branch point.
BASE_FEATURE(kWebViewXRequestedWithHeaderControl,
             "WebViewXRequestedWithHeaderControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default value of the XRequestedWith header mode when
// WebViewXRequestedWithHeaderControl is enabled. Defaults to
// |AwSettings::RequestedWithHeaderMode::NO_HEADER| Must be value declared in in
// |AwSettings::RequestedWithHeaderMode|
const base::FeatureParam<int> kWebViewXRequestedWithHeaderMode{
    &kWebViewXRequestedWithHeaderControl, "WebViewXRequestedWithHeaderMode", 0};

// If enabled zoom picker is invoked on every kGestureScrollUpdate consumed ack,
// otherwise the zoom picker is persistently shown from scroll start to scroll
// end plus the usual delay in hiding.
BASE_FEATURE(kWebViewInvokeZoomPickerOnGSU,
             "WebViewInvokeZoomPickerOnGSU",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use WebView's own Context for resource related lookups.
BASE_FEATURE(kWebViewSeparateResourceContext,
             "WebViewSeparateResourceContext",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to skip shouldInterceptRequest and other checks for prefetch
// requests.
BASE_FEATURE(kWebViewSkipInterceptsForPrefetch,
             "WebViewSkipInterceptsForPrefetch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to use initial network state during initialization to speed up
// startup.
BASE_FEATURE(kWebViewUseInitialNetworkStateAtStartup,
             "WebViewUseInitialNetworkStateAtStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables reducing webview user-agent android version and device model.
BASE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel,
             "WebViewReduceUAAndroidVersionDeviceModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables WebView crashes.
BASE_FEATURE(kWebViewEnableCrash,
             "WebViewEnableCrash",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Preloads expensive classes during WebView startup.
BASE_FEATURE(kWebViewPreloadClasses,
             "WebViewPreloadClasses",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Prefetches the native WebView code to memory during startup.
BASE_FEATURE(kWebViewPrefetchNativeLibrary,
             "WebViewPrefetchNativeLibrary",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A parameter to trigger the prefetch from the renderer instead of the browser.
const base::FeatureParam<bool> kWebViewPrefetchFromRenderer{
    &kWebViewPrefetchNativeLibrary, "WebViewPrefetchFromRenderer", false};

// Include system bars in safe-area-inset CSS environment values for WebViews
// that take up the entire screen
BASE_FEATURE(kWebViewSafeAreaIncludesSystemBars,
             "WebViewSafeAreaIncludesSystemBars",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled TYPE_SCROLLED accessibility events are sent every 100ms when user
// is scrolling irrespective of GestureScrollUpdate being consumed or not.
// If disabled events are sent on GSU consumed ack.
// Planning to keep it as kill switch in case we need to revert back to old
// default behavior.
// TODO(b/328601354): Cleanup after the change has been in stable for some time.
BASE_FEATURE(kWebViewDoNotSendAccessibilityEventsOnGSU,
             "WebViewDoNotSendAccessibilityEventsOnGSU",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables WebView's hyperlink context menu.
BASE_FEATURE(kWebViewHyperlinkContextMenu,
             "WebViewHyperlinkContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Creates a spare renderer on browser context creation.
BASE_FEATURE(kCreateSpareRendererOnBrowserContextCreation,
             "CreateSpareRendererOnBrowserContextCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for WebAuthn usage in WebViews.
BASE_FEATURE(kWebViewWebauthn,
             "WebViewWebauthn",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables RenderDocument in WebView. Note that this will only take effect
// iff both this feature flag and the content/public kRenderDocument flag is
// enabled.
BASE_FEATURE(kWebViewRenderDocument,
             "WebViewRenderDocument",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the developer hasn't overridden shouldInterceptRequest
// (or provided the async version), we short circuit (return no response)
// on the IO thread instead of calling the (empty) method on a background
// thread.
BASE_FEATURE(kWebViewShortCircuitShouldInterceptRequest,
             "WebViewShortCircuitShouldInterceptRequest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, webview chromium initialization uses the startup tasks logic
// where it runs the startup tasks asynchronously if startup is triggered from a
// background thread. Otherwise runs startup synchronously.
// Also caches any chromium startup exception and rethrows it if startup is
// retried without a restart.
BASE_FEATURE(kWebViewUseStartupTasksLogic,
             "WebViewUseStartupTasksLogic",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, records histograms relating to app's cache size.
BASE_FEATURE(kWebViewRecordAppCacheHistograms,
             "WebViewRecordAppCacheHistograms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, WebView changes the default value of the QUIC connection
// timeout, it uses the value in `WebViewUpdateQuicConnectionTimeoutSeconds`
BASE_FEATURE(kWebViewQuicConnectionTimeout,
             "WebViewQuicConnectionTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A parameter to change the quic connection timeout value, this value is in
// seconds.
const base::FeatureParam<int> kWebViewQuicConnectionTimeoutSeconds{
    &kWebViewQuicConnectionTimeout, "WebViewQuicConnectionTimeoutSeconds", 30};
// When enabled, instead of using the 20MiB as the HTTP cache
// limit, derive the value from the cache quota allocated to the app by the
// Android framework.
//
// Each code cache's limit will be half the value of the HTTP cache limit.
BASE_FEATURE(kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
             "WebViewCacheSizeLimitDerivedFromAppCacheQuota",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The multiplier that is used to compute the cache limit from the cache quota.
const base::FeatureParam<double> kWebViewCacheSizeLimitMultiplier{
    &kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
    "WebViewCacheSizeLimitMultiplier", 0.5};

// The minimum HTTP cache size limit
const base::FeatureParam<int> kWebViewCacheSizeLimitMinimum{
    &kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
    "WebViewCacheSizeLimitMinimum", 20 * 1024 * 1024};

// The maximum HTTP cache size limit
const base::FeatureParam<int> kWebViewCacheSizeLimitMaximum{
    &kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
    "WebViewCacheSizeLimitMaximum", 320 * 1024 * 1024};

// The code cache limit is this multiplier times the HTTP cache limit
const base::FeatureParam<double> kWebViewCodeCacheSizeLimitMultiplier{
    &kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
    "WebViewCodeCacheSizeLimitMultiplier", 0.5};
}  // namespace android_webview::features
