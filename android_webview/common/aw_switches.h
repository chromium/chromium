// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
#define ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_

namespace switches {

extern const char kWebViewLogJsConsoleMessages[];
extern const char kWebViewSandboxedRenderer[];
extern const char kWebViewDisableSafebrowsingSupport[];
extern const char kWebViewSafebrowsingBlockAllResources[];
extern const char kHighlightAllWebViews[];
extern const char kNetLog[];
extern const char kWebViewVerboseLogging[];
extern const char kFinchSeedExpirationAge[];
extern const char kFinchSeedIgnorePendingDownload[];
extern const char kFinchSeedNoChargingRequirement[];
extern const char kFinchSeedMinDownloadPeriod[];
extern const char kFinchSeedMinUpdatePeriod[];
extern const char kWebViewEnableModernCookieSameSite[];
extern const char kWebViewSelectiveImageInversionDarkening[];
extern const char kWebViewFencedFrames[];
extern const char kWebViewEnableTrustTokensComponent[];
extern const char kWebViewTpcdMetadaComponent[];
extern const char kWebViewFpsComponent[];
extern const char kWebViewForceDisable3pcs[];
extern const char kWebViewForceCrashJava[];
extern const char kWebViewForceCrashNative[];
extern const char kWebViewUseSeparateResourceContext[];
extern const char kDebugBsa[];
extern const char kWebViewInterceptedCookieHeader[];

}  // namespace switches

#endif  // ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
