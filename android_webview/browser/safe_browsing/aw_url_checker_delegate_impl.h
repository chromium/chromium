// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_URL_CHECKER_DELEGATE_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_URL_CHECKER_DELEGATE_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

class AwSafeBrowsingUIManager;
class AwSafeBrowsingAllowlistManager;
struct AwWebResourceRequest;

// Lifetime: Singleton
class AwUrlCheckerDelegateImpl : public safe_browsing::UrlCheckerDelegate {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview
  enum class SafeBrowsingAction {
    SHOW_INTERSTITIAL,
    PROCEED,
    BACK_TO_SAFETY,
  };

  AwUrlCheckerDelegateImpl(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      AwSafeBrowsingAllowlistManager* allowlist_manager);

  AwUrlCheckerDelegateImpl(const AwUrlCheckerDelegateImpl&) = delete;
  AwUrlCheckerDelegateImpl& operator=(const AwUrlCheckerDelegateImpl&) = delete;

 private:
  ~AwUrlCheckerDelegateImpl() override;

  // Implementation of UrlCheckerDelegate:
  void MaybeDestroyNoStatePrefetchContents(
      content::WebContents::OnceGetter web_contents_getter) override;
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
      int child_id,
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
  const safe_browsing::SBThreatTypeSet& GetThreatTypes() override;
  safe_browsing::SafeBrowsingDatabaseManager* GetDatabaseManager() override;
  safe_browsing::BaseUIManager* GetUIManager() override;

  static void StartApplicationResponse(
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      const security_interstitials::UnsafeResource& resource,
      const AwWebResourceRequest& request);

  // Follow the application's response to WebViewClient#onSafeBrowsingHit(). If
  // the action is PROCEED or BACK_TO_SAFETY, then |reporting| will determine if
  // we should send an extended report. Otherwise, |reporting| determines if we
  // should allow showing the reporting checkbox or not.
  static void DoApplicationResponse(
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      const security_interstitials::UnsafeResource& resource,
      const AwWebResourceRequest& request,
      SafeBrowsingAction action,
      bool reporting);

  static void StartDisplayingDefaultBlockingPage(
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      const security_interstitials::UnsafeResource& resource);

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<AwSafeBrowsingUIManager> ui_manager_;
  safe_browsing::SBThreatTypeSet threat_types_;
  raw_ptr<AwSafeBrowsingAllowlistManager> allowlist_manager_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_URL_CHECKER_DELEGATE_IMPL_H_
