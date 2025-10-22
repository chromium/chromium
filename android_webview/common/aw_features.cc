// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "services/network/public/cpp/features.h"

namespace android_webview::features {

// Alphabetical:

// Kill switch for Profile.addQuicHints.
BASE_FEATURE(kWebViewAddQuicHints, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable auto granting storage access API requests. This will be done
// if a relationship is detected between the app and the website.
BASE_FEATURE(kWebViewAutoSAA, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable back/forward cache support in WebView. Note that this will only take
// effect iff both this feature flag and the content/public kBackForwardCache
// flag is enabled.
BASE_FEATURE(kWebViewBackForwardCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Allow apps to configure the renderer library prefetching behaviour.
BASE_FEATURE(kWebViewConfigurableLibraryPrefetch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable loading include statements when checking digital asset links
BASE_FEATURE(kWebViewDigitalAssetLinksLoadIncludes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables partitioned cookies by default on WebView. This can still be
// overridden by our `setPartitionedCookiesEnabled` Android X API.
BASE_FEATURE(kWebViewDisableCHIPS, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables draining the WebView prefetch queue (for prefetches triggered from
// background thread) during WebView instance initialization and before
// WebView#loadUrl().
// TODO(crbug.com/419251646): remove for M139.
BASE_FEATURE(kWebViewDrainPrefetchQueueDuringInit,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable JS FileSystemAccess API.
// This flag is set by WebView internal code based on an app's targetSdkVersion.
// It is enabled for version B+. The default value here is not relevant, and is
// not expected to be manually changed.
// TODO(b/364980165): Flag can be removed when SDK versions prior to B are no
// longer supported.
BASE_FEATURE(kWebViewFileSystemAccess, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable ignoring duplicate navigations in WebView. Note that this will only
// take effect if both this feature flag and the content/public
// kIgnoreDuplicateNavs flag is enabled.
BASE_FEATURE(kWebViewIgnoreDuplicateNavs, base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, passive mixed content (Audio/Video/Image subresources loaded
// over HTTP on HTTPS sites) will be autoupgraded to HTTPS, and the load will be
// blocked if the resource fails to load over HTTPS. This only affects apps that
// set the mixed content mode to MIXED_CONTENT_COMPATIBILITY_MODE, autoupgrades
// are always disabled for MIXED_CONTENT_NEVER_ALLOW and
// MIXED_CONTENT_ALWAYS_ALLOW modes.
BASE_FEATURE(kWebViewMixedContentAutoupgrades,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables WebView audio to be muted using an API.
BASE_FEATURE(kWebViewMuteAudio, base::FEATURE_ENABLED_BY_DEFAULT);

// A Feature used for WebView variations tests. Not used in production. Please
// do not clean up this stale feature: we intentionally keep this feature flag
// around for testing purposes.
BASE_FEATURE(kWebViewTestFeature, base::FEATURE_DISABLED_BY_DEFAULT);

// Use WebView's nonembedded MetricsUploadService to upload UMA metrics instead
// of sending it directly to GMS-core.
BASE_FEATURE(kWebViewUseMetricsUploadService,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use WebView's nonembedded MetricsUploadService to upload UMA metrics instead
// of sending it directly to GMS-core when running within the SDK Runtime.
BASE_FEATURE(kWebViewUseMetricsUploadServiceOnlySdkRuntime,
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
BASE_FEATURE(kWebViewUnreducedProductVersion, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled zoom picker is invoked on every kGestureScrollUpdate consumed ack,
// otherwise the zoom picker is persistently shown from scroll start to scroll
// end plus the usual delay in hiding.
BASE_FEATURE(kWebViewInvokeZoomPickerOnGSU, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to skip shouldInterceptRequest and other checks for prefetch
// requests.
BASE_FEATURE(kWebViewSkipInterceptsForPrefetch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to use initial network state during initialization to speed up
// startup.
BASE_FEATURE(kWebViewUseInitialNetworkStateAtStartup,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables reducing webview user-agent android version and device model.
BASE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables WebView crashes.
BASE_FEATURE(kWebViewEnableCrash, base::FEATURE_DISABLED_BY_DEFAULT);

// Preloads expensive classes during WebView startup.
BASE_FEATURE(kWebViewPreloadClasses, base::FEATURE_ENABLED_BY_DEFAULT);

// Prefetches the native WebView code to memory during startup.
BASE_FEATURE(kWebViewPrefetchNativeLibrary, base::FEATURE_ENABLED_BY_DEFAULT);

// A parameter to trigger the prefetch from the renderer instead of the browser.
const base::FeatureParam<bool> kWebViewPrefetchFromRenderer{
    &kWebViewPrefetchNativeLibrary, "WebViewPrefetchFromRenderer", true};

// Include system bars in safe-area-inset CSS environment values for WebViews
// that take up the entire screen
BASE_FEATURE(kWebViewSafeAreaIncludesSystemBars,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled TYPE_SCROLLED accessibility events are sent every 100ms when user
// is scrolling irrespective of GestureScrollUpdate being consumed or not.
// If disabled events are sent on GSU consumed ack.
// Planning to keep it as kill switch in case we need to revert back to old
// default behavior.
// TODO(b/328601354): Cleanup after the change has been in stable for some time.
BASE_FEATURE(kWebViewDoNotSendAccessibilityEventsOnGSU,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables WebView's hyperlink context menu.
BASE_FEATURE(kWebViewHyperlinkContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);

// Creates a spare renderer on browser context creation.
BASE_FEATURE(kCreateSpareRendererOnBrowserContextCreation,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for WebAuthn usage in WebViews.
BASE_FEATURE(kWebViewWebauthn, base::FEATURE_ENABLED_BY_DEFAULT);

// This enables RenderDocument in WebView. Note that this will only take effect
// iff both this feature flag and the content/public kRenderDocument flag is
// enabled.
BASE_FEATURE(kWebViewRenderDocument, base::FEATURE_DISABLED_BY_DEFAULT);

// This enables getViewportInsetBottom which is used to resize the visual
// viewport according to both the visible area of the WebView and any IME
// overlap.
BASE_FEATURE(kWebViewReportImeInsets, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, if the developer hasn't overridden shouldInterceptRequest
// (or provided the async version), we short circuit (return no response)
// on the IO thread instead of calling the (empty) method on a background
// thread.
BASE_FEATURE(kWebViewShortCircuitShouldInterceptRequest,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, WebView disables MSAA and doesn't auto sharpen mip-mapped
// textures on very large screen devices (such as TVs). The exact criteria for
// what qualifies for this can be found in AwGrContextOptionsProvider.java.
BASE_FEATURE(kWebViewUseRenderingHeuristic, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, webview chromium initialization uses the startup tasks logic
// where it runs the startup tasks asynchronously if startup is triggered from a
// background thread. Otherwise runs startup synchronously.
// Also caches any chromium startup exception and rethrows it if startup is
// retried without a restart.
// Note: WebViewUseStartupTasksLogicP2 and kWebViewStartupTasksYieldToNative
// also enable the same behaviour as this flag.
BASE_FEATURE(kWebViewUseStartupTasksLogic, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, observe WebView movements with a PreDrawListener and use those
// events to re-calculate the visual viewport of the web contents.
BASE_FEATURE(kWebViewUseViewPositionObserverForInsets,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, records histograms relating to app's cache size.
BASE_FEATURE(kWebViewRecordAppCacheHistograms,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, WebView changes the default value of the QUIC connection
// timeout, it uses the value in `WebViewUpdateQuicConnectionTimeoutSeconds`
BASE_FEATURE(kWebViewQuicConnectionTimeout, base::FEATURE_ENABLED_BY_DEFAULT);

// A parameter to change the quic connection timeout value, this value is in
// seconds.
const base::FeatureParam<int> kWebViewQuicConnectionTimeoutSeconds{
    &kWebViewQuicConnectionTimeout, "WebViewQuicConnectionTimeoutSeconds", 300};
// When enabled, instead of using the 20MiB as the HTTP cache
// limit, derive the value from the cache quota allocated to the app by the
// Android framework.
//
// Each code cache's limit will be half the value of the HTTP cache limit.
BASE_FEATURE(kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
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

// Connect to the non-embedded components provider from a background thread.
BASE_FEATURE(kWebViewConnectToComponentProviderInBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables phase 2 of using startup tasks logic for webview chromium
// initialization which starts browser process asynchronously, when starting
// webview asynchronously.
// Note: This also enables the same behaviour as WebViewUseStartupTasksLogic and
// WebViewStartupTasksYieldToNative with minor differences.
BASE_FEATURE(kWebViewUseStartupTasksLogicP2, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables running native startup tasks asynchronously if WebView startup is
// asynchronous.
// Note:This also enables the same behaviour as WebViewUseStartupTasksLogic and
// WebViewUseStartupTasksLogicP2, with minor additions.
BASE_FEATURE(kWebViewStartupTasksYieldToNative,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This results in the metric logging being run on a separate thread and
// blocking until the results are retrieved.
// When this is disabled, logging is initiated on the main thread and a success
// status is reported to the chromium metrics service immediately.
BASE_FEATURE(kAndroidMetricsAsyncMetricLogging,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reduce when the app's copy of the finch seed expires. This makes WebView more
// aggressive in requesting a new copy of its finch seed.
BASE_FEATURE(kWebViewReducedSeedExpiration, base::FEATURE_DISABLED_BY_DEFAULT);

// This flag reduces the minimum amount of time before WebView can request a new
// seed. This, in conjunction with kWebViewReducedSeedExpiration, should mean
// more up-to-date copies of finch seeds.
BASE_FEATURE(kWebViewReducedSeedRequestPeriod,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables early Java startup tracing, which unconditionally collects timing
// information and queues runnables to emit the trace events once Perfetto is
// initialized. This flag does not affect tracing in native code.
BASE_FEATURE(kWebViewEarlyStartupTracing, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables early Perfetto init. This will initialize Perfetto as soon as the
// native library is loaded, which should make it available by the time we start
// calling content code.
BASE_FEATURE(kWebViewEarlyPerfettoInit, base::FEATURE_DISABLED_BY_DEFAULT);

// Caches reflective methods in AndroidX instead of looking them up every time.
// This should make calling AndroidX methods faster.
BASE_FEATURE(kWebViewCacheBoundaryInterfaceMethods,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, opts in WebView to GMSCore's bindService optimizations.
BASE_FEATURE(kWebViewOptInToGmsBindServiceOptimization,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Moves some of the work that is being run during
// `startChromium` to be done beforehand during WebView provider
// initialization. This is expected to improve startup performance especially
// when async startup takes place.
BASE_FEATURE(kWebViewMoveWorkToProviderInit, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the temporary cookie manager used before WebView startup is
// bypassed. If WebView isn't already started up, calling
// `CookieManager.getInstance()` will trigger WebView startup on the main looper
// and wait for startup to complete.
BASE_FEATURE(kWebViewBypassProvisionalCookieManager,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace android_webview::features
