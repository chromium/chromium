// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_URL_CHECKER_DELEGATE_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_URL_CHECKER_DELEGATE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"

namespace android_webview {

class AwSafeBrowsingUIManager;
class AwSafeBrowsingWhitelistManager;
struct AwWebResourceRequest;

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
      AwSafeBrowsingWhitelistManager* whitelist_manager);

 private:
  ~AwUrlCheckerDelegateImpl() override;

  // Implementation of UrlCheckerDelegate:
  void MaybeDestroyPrerenderContents(
      const base::Callback<content::WebContents*()>& web_contents_getter)
      override;
  void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_main_frame,
      bool has_user_gesture) override;
  bool IsUrlWhitelisted(const GURL& url) override;
  bool ShouldSkipRequestCheck(content::ResourceContext* resource_context,
                              const GURL& original_url,
                              int frame_tree_node_id,
                              int render_process_id,
                              int render_frame_id,
                              bool originated_from_service_worker) override;
  void NotifySuspiciousSiteDetected(
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
      SafeBrowsingAction action,
      bool reporting);

  static void StartDisplayingDefaultBlockingPage(
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      const security_interstitials::UnsafeResource& resource);

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<AwSafeBrowsingUIManager> ui_manager_;
  safe_browsing::SBThreatTypeSet threat_types_;
  AwSafeBrowsingWhitelistManager* whitelist_manager_;

  DISALLOW_COPY_AND_ASSIGN(AwUrlCheckerDelegateImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_URL_CHECKER_DELEGATE_IMPL_H_
