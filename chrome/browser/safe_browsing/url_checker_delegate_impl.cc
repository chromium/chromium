// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/prerender/browser/prerender_contents.h"
#include "components/prerender/common/prerender_final_status.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/triggers/suspicious_site_trigger.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

namespace safe_browsing {
namespace {

// Destroys the prerender contents associated with the web_contents, if any.
void DestroyPrerenderContents(
    content::WebContents::OnceGetter web_contents_getter) {
  content::WebContents* web_contents = std::move(web_contents_getter).Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::ChromePrerenderContentsDelegate::FromWebContents(
            web_contents);
    if (prerender_contents)
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
  }
}

void CreateSafeBrowsingUserInteractionObserver(
    const content::WebContents::Getter& web_contents_getter,
    const security_interstitials::UnsafeResource& resource,
    bool is_main_frame,
    scoped_refptr<SafeBrowsingUIManager> ui_manager) {
  content::WebContents* web_contents = web_contents_getter.Run();
  // Don't delay the interstitial for prerender pages and portals.
  if (!web_contents ||
      prerender::ChromePrerenderContentsDelegate::FromWebContents(
          web_contents) ||
      web_contents->IsPortal()) {
    SafeBrowsingUIManager::StartDisplayingBlockingPage(ui_manager, resource);
    return;
  }
#if defined(OS_ANDROID)
  // Don't delay the interstitial for Chrome Custom Tabs.
  auto* tab_android = TabAndroid::FromWebContents(web_contents);
  if (tab_android && tab_android->IsCustomTab()) {
    SafeBrowsingUIManager::StartDisplayingBlockingPage(ui_manager, resource);
    return;
  }
#endif
  SafeBrowsingUserInteractionObserver::CreateForWebContents(
      web_contents, resource, is_main_frame, ui_manager);
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
    content::WebContents::OnceGetter web_contents_getter) {
  // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DestroyPrerenderContents,
                                std::move(web_contents_getter)));
}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SafeBrowsingUIManager::StartDisplayingBlockingPage,
                     ui_manager_, resource));
}

// Starts displaying the SafeBrowsing interstitial page.
void UrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource,
        bool is_main_frame) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CreateSafeBrowsingUserInteractionObserver,
                                resource.web_contents_getter, resource,
                                is_main_frame, ui_manager_));
}

bool UrlCheckerDelegateImpl::IsUrlAllowlisted(const GURL& url) {
  return false;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
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
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NotifySuspiciousSiteTriggerDetected,
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
