// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_CHROME_PREFETCH_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_CHROME_PREFETCH_SERVICE_DELEGATE_H_

#include <string>

#include "base/time/time.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class Profile;
class PrefetchOriginDecider;

class ChromePrefetchServiceDelegate : public content::PrefetchServiceDelegate {
 public:
  explicit ChromePrefetchServiceDelegate(
      content::BrowserContext* browser_context);
  ~ChromePrefetchServiceDelegate() override;

  ChromePrefetchServiceDelegate(const ChromePrefetchServiceDelegate&) = delete;
  ChromePrefetchServiceDelegate& operator=(
      const ChromePrefetchServiceDelegate&) = delete;

  // content::PrefetchServiceDelegate
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
  // The profile that |this| is associated with.
  raw_ptr<Profile> profile_;

  // Tracks "Retry-After" responses, and determines whether new prefetches are
  // eligible based on those responses.
  std::unique_ptr<PrefetchOriginDecider> origin_decider_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_CHROME_PREFETCH_SERVICE_DELEGATE_H_
