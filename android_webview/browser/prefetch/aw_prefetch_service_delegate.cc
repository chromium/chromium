// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_service_delegate.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/notreached.h"

namespace android_webview {

AwPrefetchServiceDelegate::AwPrefetchServiceDelegate(
    AwBrowserContext* browser_context)
    : browser_context_(*browser_context) {}

AwPrefetchServiceDelegate::~AwPrefetchServiceDelegate() = default;

std::string AwPrefetchServiceDelegate::GetMajorVersionNumber() {
  NOTREACHED() << "Only used for isolated network context. WebView doesn't use "
                  "an isolated network context for app triggered prefetching.";
}

std::string AwPrefetchServiceDelegate::GetAcceptLanguageHeader() {
  NOTREACHED() << "Only used for isolated network context. WebView doesn't use "
                  "an isolated network context for app triggered prefetching.";
}

GURL AwPrefetchServiceDelegate::GetDefaultPrefetchProxyHost() {
  // Used for prefetch proxy config (which is always constructed). WebView
  // doesn't use a proxy for app triggered prefetching. If WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return GURL("");
}

std::string AwPrefetchServiceDelegate::GetAPIKey() {
  // Used for prefetch proxy config (which is always constructed). WebView
  // doesn't use a proxy for app triggered prefetching. If WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return "";
}

GURL AwPrefetchServiceDelegate::GetDefaultDNSCanaryCheckURL() {
  // Used for prefetch proxy config (which is always constructed). WebView
  // doesn't use a proxy for app triggered prefetching. If WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return GURL("");
}

GURL AwPrefetchServiceDelegate::GetDefaultTLSCanaryCheckURL() {
  // Used for prefetch proxy config (which is always constructed). WebView
  // doesn't use a proxy for app triggered prefetching. If WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return GURL("");
}

void AwPrefetchServiceDelegate::ReportOriginRetryAfter(
    const GURL& url,
    base::TimeDelta retry_after) {
  // TODO (crbug.com/369313220) : Implement retry-after logic.
}

bool AwPrefetchServiceDelegate::IsOriginOutsideRetryAfterWindow(
    const GURL& url) {
  // TODO (crbug.com/369313220) : Implement retry-after logic.
  return true;
}

void AwPrefetchServiceDelegate::ClearData() {
  // TODO (crbug.com/369313220) : Implement retry-after logic.
}

bool AwPrefetchServiceDelegate::DisableDecoysBasedOnUserSettings() {
  // Decoys are not supported within app-triggered prefetching.
  // However, if WebView ever adds support for non-app triggered prefetching, we
  // may need to revisit the value returned here.
  return true;
}

content::PreloadingEligibility
AwPrefetchServiceDelegate::IsSomePreloadingEnabled() {
  // Prefetching within WebView is currently only app-triggered so by default we
  // return |PreloadingEligibility::kEligible|. However, if WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return content::PreloadingEligibility::kEligible;
}

bool AwPrefetchServiceDelegate::IsPreloadingPrefEnabled() {
  // This flag is not used within app triggered prefetching (which is the only
  // prefetching WebView currently supports). However, if WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return false;
}

bool AwPrefetchServiceDelegate::IsDataSaverEnabled() {
  // Data saver is not considered within app triggered prefetching (which is the
  // only prefetching WebView currently supports). However, if WebView ever adds
  // support for non-app triggered prefetching, we may need to revisit the value
  // returned here.
  return false;
}

bool AwPrefetchServiceDelegate::IsBatterySaverEnabled() {
  // Battery saver is not considered within app triggered prefetching (which is
  // the only prefetching WebView currently supports). However, if WebView ever
  // adds support for non-app triggered prefetching, we may need to revisit the
  // value returned here.
  return false;
}

bool AwPrefetchServiceDelegate::IsExtendedPreloadingEnabled() {
  // WebView app initiated prefetching does no support extended preloading.
  // However, if WebView ever adds support for non-app triggered prefetching, we
  // may need to revisit the value returned here.
  return false;
}

bool AwPrefetchServiceDelegate::IsDomainInPrefetchAllowList(
    const GURL& referring_url) {
  // WebView app initiated prefetching does not use prefetch allow lists.
  // However, if WebView ever adds support for non-app triggered prefetching, we
  // may need to revisit the value returned here.
  return false;
}

bool AwPrefetchServiceDelegate::IsContaminationExempt(
    const GURL& referring_url) {
  // WebView app initiated prefetching does not use an isolated network context.
  // However, if WebView ever adds support for non-app triggered prefetching, we
  // may need to revisit the value returned here.
  return false;
}

void AwPrefetchServiceDelegate::OnPrefetchLikely(
    content::WebContents* web_contents) {
  // Only used for renderer initiated prefetching which WebView doesn't
  // currently support.
}

}  // namespace android_webview
