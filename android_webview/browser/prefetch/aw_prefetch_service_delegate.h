// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_SERVICE_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_SERVICE_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "content/public/browser/prefetch_service_delegate.h"

namespace android_webview {

class AwBrowserContext;

class AwPrefetchServiceDelegate : public content::PrefetchServiceDelegate {
 public:
  explicit AwPrefetchServiceDelegate(AwBrowserContext* browser_context);
  ~AwPrefetchServiceDelegate() override;

  AwPrefetchServiceDelegate(const AwPrefetchServiceDelegate&) = delete;
  AwPrefetchServiceDelegate& operator=(const AwPrefetchServiceDelegate&) =
      delete;

  /// content::PrefetchServiceDelegate
  std::string GetMajorVersionNumber() override;
  std::string GetAcceptLanguageHeader() override;
  GURL GetDefaultPrefetchProxyHost() override;
  std::string GetAPIKey() override;
  GURL GetDefaultDNSCanaryCheckURL() override;
  GURL GetDefaultTLSCanaryCheckURL() override;
  void ReportOriginRetryAfter(const GURL& url,
                              base::TimeDelta retry_after) override;
  bool IsOriginOutsideRetryAfterWindow(const GURL& url) override;
  void ClearData() override;
  bool DisableDecoysBasedOnUserSettings() override;
  content::PreloadingEligibility IsSomePreloadingEnabled() override;
  bool IsPreloadingPrefEnabled() override;
  bool IsDataSaverEnabled() override;
  bool IsBatterySaverEnabled() override;
  bool IsExtendedPreloadingEnabled() override;
  bool IsDomainInPrefetchAllowList(const GURL& referring_url) override;
  bool IsContaminationExempt(const GURL& referring_url) override;
  void OnPrefetchLikely(content::WebContents* web_contents) override;

 private:
  // The browser context that |this| is associated with.
  raw_ref<AwBrowserContext> browser_context_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_SERVICE_DELEGATE_H_
