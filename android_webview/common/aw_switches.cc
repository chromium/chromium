// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_switches.h"

namespace switches {

const char kWebViewLogJsConsoleMessages[] = "webview-log-js-console-messages";

const char kWebViewSandboxedRenderer[] = "webview-sandboxed-renderer";

// used to disable safebrowsing functionality in webview
const char kWebViewDisableSafebrowsingSupport[] =
    "webview-disable-safebrowsing-support";

// Enables SafeBrowsing and causes WebView to treat all resources as malicious.
// Use care: this will block all resources from loading.
const char kWebViewSafebrowsingBlockAllResources[] =
    "webview-safebrowsing-block-all-resources";

// Highlight the contents (including web contents) of all WebViews with a yellow
// tint. This is useful for identifying WebViews in an Android application.
const char kHighlightAllWebViews[] = "highlight-all-webviews";

// Enable net logging from WebView. This captures network activity for debugging
// purposes, and stores the files in DevUi.
const char kNetLog[] = "net-log";

// WebView will log additional debugging information to logcat, such as
// variations and commandline state.
const char kWebViewVerboseLogging[] = "webview-verbose-logging";

// The length of time in seconds that an app's copy of the variations seed
// should be considered fresh. If an app's seed is older than this, a new seed
// will be requested from WebView's IVariationsSeedServer.
const char kFinchSeedExpirationAge[] = "finch-seed-expiration-age";

// Forces WebView's service to always schedule a new variations seed download
// job, even if one is already pending.
const char kFinchSeedIgnorePendingDownload[] =
    "finch-seed-ignore-pending-download";

// Forces WebView's service to always schedule a new variations seed download
// job, even if the device is not charging. Note this switch may be necessary
// for testing on Android emulators as these are not always considered to be
// charging.
const char kFinchSeedNoChargingRequirement[] =
    "finch-seed-no-charging-requirement";

// The minimum amount of time in seconds that WebView's service will wait
// between two variations seed downloads from the variations server.
const char kFinchSeedMinDownloadPeriod[] = "finch-seed-min-download-period";

// The minimum amount of time in seconds that the embedded WebView
// implementation will wait between two requests to WebView's service for a new
// variations seed.
const char kFinchSeedMinUpdatePeriod[] = "finch-seed-min-update-period";

// Enables modern SameSite cookie behavior (as opposed to legacy behavior). This
// is used for WebView versions prior to when the modern behavior will be
// enabled by default. This enables the same-site-by-default-cookies,
// cookies-without-SameSite-must-be-secure, and schemeful-same-site features.
const char kWebViewEnableModernCookieSameSite[] =
    "webview-enable-modern-cookie-same-site";

// Enables use selective image inversion to automatically darken page, it will
// be used when WebView is in dark mode, but website doesn't provide dark style.
const char kWebViewSelectiveImageInversionDarkening[] =
    "webview-selective-image-inversion-darkening";

// Enables FencedFrames. This also enables PrivacySandboxAdsAPIsOverride.
const char kWebViewFencedFrames[] = "webview-fenced-frames";

// Enables downloading TrustTokenKeyCommitmentsComponent by the component
// updater downloading service in nonembedded WebView. See
// https://crbug.com/1170468.
const char kWebViewEnableTrustTokensComponent[] =
    "webview-enable-trust-tokens-component";

// Enables downloading TpcdMetadataComponentInstallerPolicy by the component
// updater downloading service in nonembedded WebView.
const char kWebViewTpcdMetadaComponent[] = "webview-tpcd-metadata-component";

// Enables downloading FirstPartySetsComponentInstallerPolicy by the component
// updater downloading service in nonembedded WebView.
const char kWebViewFpsComponent[] = "webview-fps-component";

// Force disables 3rd party cookie for all apps.
const char kWebViewForceDisable3pcs[] = "webview-force-disable-3pcs";

// Enables crashes during WebView startup in the Java layer
const char kWebViewForceCrashJava[] = "webview-force-crash-java";

// Enables crashes during WebView startup in the Native layer
const char kWebViewForceCrashNative[] = "webview-force-crash-native";

// Use WebView's context for resource lookups instead of the embedding app's.
const char kWebViewUseSeparateResourceContext[] =
    "webview-use-separate-resource-context";

// Override and enable features useful for BSA library testing/debugging.
const char kDebugBsa[] = "debug-bsa";

// When enabled, the cookie header will be included in the request headers
// for shouldInterceptRequest.
const char kWebViewInterceptedCookieHeader[] =
    "webview-intercepted-cookie-header";

}  // namespace switches
