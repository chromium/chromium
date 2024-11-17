// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

class SafeBrowsingUIManager;

class UrlCheckerDelegateImpl : public UrlCheckerDelegate {
 public:
  UrlCheckerDelegateImpl(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

  UrlCheckerDelegateImpl(const UrlCheckerDelegateImpl&) = delete;
  UrlCheckerDelegateImpl& operator=(const UrlCheckerDelegateImpl&) = delete;

 private:
  ~UrlCheckerDelegateImpl() override;

  // Implementation of UrlCheckerDelegate:
  void MaybeDestroyNoStatePrefetchContents(
      content::WebContents::OnceGetter web_contents_getter) override;
  // Only uses |resource| and ignores the rest of parameters.
  void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool has_user_gesture) override;
  void StartObservingInteractionsForDelayedBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource) override;

  bool IsUrlAllowlisted(const GURL& url) override;
  void SetPolicyAllowlistDomains(
      const std::vector<std::string>& allowlist_domains) override;
  bool ShouldSkipRequestCheck(
      const GURL& original_url,
      int frame_tree_node_id,
      int render_process_id,
      base::optional_ref<const base::UnguessableToken> render_frame_token,
      bool originated_from_service_worker) override;
  void NotifySuspiciousSiteDetected(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
  void SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
      std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest> report,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
  bool AreBackgroundHashRealTimeSampleLookupsAllowed(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
  const SBThreatTypeSet& GetThreatTypes() override;
  SafeBrowsingDatabaseManager* GetDatabaseManager() override;
  BaseUIManager* GetUIManager() override;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  // A list of domains allowlisted by the enterprise policy.
  std::vector<std::string> allowlist_domains_;
  SBThreatTypeSet threat_types_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
