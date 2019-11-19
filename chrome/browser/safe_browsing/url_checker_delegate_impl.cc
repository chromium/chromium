// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/suspicious_site_trigger.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/features.h"

namespace safe_browsing {
namespace {

// Destroys the prerender contents associated with the web_contents, if any.
void DestroyPrerenderContents(
    const base::Callback<content::WebContents*()>& web_contents_getter) {
  content::WebContents* web_contents = web_contents_getter.Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents)
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
  }
}

void StartDisplayingBlockingPage(
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    } else {
      // With committed interstitials, if this is a main frame load, we need to
      // get the navigation URL and referrer URL from the navigation entry now,
      // since they are required for threat reporting, and the entry will be
      // destroyed once the request is failed.
      if (base::FeatureList::IsEnabled(kCommittedSBInterstitials) &&
          resource.IsMainPageLoadBlocked()) {
        content::NavigationEntry* entry =
            web_contents->GetController().GetPendingEntry();
        if (entry) {
          security_interstitials::UnsafeResource resource_copy(resource);
          resource_copy.navigation_url = entry->GetURL();
          resource_copy.referrer_url = entry->GetReferrer().url;
          ui_manager->DisplayBlockingPage(resource_copy);
          return;
        }
      }
      ui_manager->DisplayBlockingPage(resource);
      return;
    }
  }

  // Tab is gone or it's being prerendered.
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(resource.callback, false));
}

}  // namespace

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      threat_types_(CreateSBThreatTypeSet({
// TODO(crbug.com/835961): Enable on Android when list is available.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
        safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE,
#endif
            safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
            safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
            safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
            safe_browsing::SB_THREAT_TYPE_BILLING
      })) {
}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyPrerenderContents(
    const base::Callback<content::WebContents*()>& web_contents_getter) {
  // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&DestroyPrerenderContents, web_contents_getter));
}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&StartDisplayingBlockingPage, ui_manager_, resource));
}

bool UrlCheckerDelegateImpl::IsUrlWhitelisted(const GURL& url) {
  return false;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    content::ResourceContext* resource_context,
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
  return false;
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&NotifySuspiciousSiteTriggerDetected,
                                web_contents_getter));
}

const SBThreatTypeSet& UrlCheckerDelegateImpl::GetThreatTypes() {
  return threat_types_;
}

SafeBrowsingDatabaseManager* UrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

BaseUIManager* UrlCheckerDelegateImpl::GetUIManager() {
  return ui_manager_.get();
}

}  // namespace safe_browsing
