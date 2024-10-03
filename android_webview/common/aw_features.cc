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

// Kill switch for adding CHECKs to loading pak files.
BASE_FEATURE(kWebViewCheckPakFileDescriptors,
             "WebViewCheckPakFileDescriptors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable loading include statements when checking digital asset links
BASE_FEATURE(kWebViewDigitalAssetLinksLoadIncludes,
             "WebViewDigitalAssetLinksLoadIncludes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows JS DataTransfer Files from content URIs in drag-drop.
BASE_FEATURE(kWebViewDragDropFiles,
             "WebViewDragDropFiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Cache origins which have camera/mic permissions approved to allow subsequent
// calls to enumerate devices to return device labels.
BASE_FEATURE(kWebViewEnumerateDevicesCache,
             "WebViewEnumerateDevicesCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable JS FileSystemAccess API.
// TODO(b/364980165): Add targetSdkVersion checks before enabling.
BASE_FEATURE(kWebViewFileSystemAccess,
             "WebViewFileSystemAccess",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebView to automatically darken the page in FORCE_DARK_AUTO mode if
// the app's theme is dark.
BASE_FEATURE(kWebViewForceDarkModeMatchTheme,
             "WebViewForceDarkModeMatchTheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewHitTestInBlinkOnTouchStart,
             "WebViewHitTestInBlinkOnTouchStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature parameter for `network::features::kMaskedDomainList` that sets the
// exclusion criteria for defining which domains are excluded from the
// Masked Domain List for WebView.
//
// Exclusion criteria can assume values from `WebviewExclusionPolicy`.
const base::FeatureParam<int> kWebViewIpProtectionExclusionCriteria{
    &network::features::kMaskedDomainList,
    "WebViewIpProtectionExclusionCriteria",
    /*WebviewExclusionPolicy::kNone*/ 0};

// Enable display cutout support for Android P and above.
BASE_FEATURE(kWebViewDisplayCutout,
             "WebViewDisplayCutout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Fetch Hand Writing icon lazily.
BASE_FEATURE(kWebViewLazyFetchHandWritingIcon,
             "WebViewLazyFetchHandWritingIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Only allow extra headers added via loadUrl() to be sent to the original
// origin; strip them from the request if a cross-origin redirect occurs.
BASE_FEATURE(kWebViewExtraHeadersSameOriginOnly,
             "WebViewExtraHeadersSameOriginOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to record size of the embedding app's data directory to the UMA
// histogram Android.WebView.AppDataDirectorySize.
BASE_FEATURE(kWebViewRecordAppDataDirectorySize,
             "WebViewRecordAppDataDirectorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Flag to restrict main frame Web Content to verified web content. Verification
// happens via Digital Asset Links.
BASE_FEATURE(kWebViewRestrictSensitiveContent,
             "WebViewRestrictSensitiveContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enable raster in wide color gamut for apps that use webview in a wide color
// gamut activity.
BASE_FEATURE(kWebViewWideColorGamutSupport,
             "WebViewWideColorGamutSupport",
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

// This enables image drage out for Webview.
BASE_FEATURE(kWebViewImageDrag,
             "WebViewImageDrag",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled zoom picker is invoked on every kGestureScrollUpdate consumed ack,
// otherwise the zoom picker is persistently shown from scroll start to scroll
// end plus the usual delay in hiding.
BASE_FEATURE(kWebViewInvokeZoomPickerOnGSU,
             "WebViewInvokeZoomPickerOnGSU",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use WebView's own Context for resource related lookups.
BASE_FEATURE(kWebViewSeparateResourceContext,
             "WebViewSeparateResourceContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use initial network state during initialization to speed up
// startup.
BASE_FEATURE(kWebViewUseInitialNetworkStateAtStartup,
             "WebViewUseInitialNetworkStateAtStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables zoom keyboard shortcuts for zoom-in, zoom-out and zoom reset.
BASE_FEATURE(kWebViewZoomKeyboardShortcuts,
             "WebViewZoomKeyboardShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enables reducing webview user-agent android version and device model.
BASE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel,
             "WebViewReduceUAAndroidVersionDeviceModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This enables WebView crashes.
BASE_FEATURE(kWebViewEnableCrash,
             "WebViewEnableCrash",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the built-in DNS resolver (Async DNS) on WebView.
BASE_FEATURE(kWebViewAsyncDns,
             "WebViewAsyncDns",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Preloads expensive classes during WebView startup.
BASE_FEATURE(kWebViewPreloadClasses,
             "WebViewPreloadClasses",
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

// Auto-grants the `SANITIZED_CLIPBOARD_WRITE` permission.
// This flag is introduced as a kill-switch in case the change leads
// to problems.
// TODO(https://crbug.com/362460435) Remove after launch.
BASE_FEATURE(kWebViewAutoGrantSanitizedClipboardWrite,
             "WebViewAutoGrantSanitizedClipboardWrite",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace android_webview::features
