// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/triggers/suspicious_site_trigger.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/android/tab_android.h"
#endif

namespace safe_browsing {
namespace {

// Destroys the NoStatePrefetch contents associated with the web_contents, if
// any.
void DestroyNoStatePrefetchContents(
    content::WebContents::OnceGetter web_contents_getter) {
  content::WebContents* web_contents = std::move(web_contents_getter).Run();
  if (web_contents) {
    prerender::NoStatePrefetchContents* no_state_prefetch_contents =
        prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
            web_contents);
    if (no_state_prefetch_contents) {
      no_state_prefetch_contents->Destroy(
          prerender::FINAL_STATUS_SAFE_BROWSING);
    }
  }
}

void CreateSafeBrowsingUserInteractionObserver(
    const security_interstitials::UnsafeResource& resource,
    scoped_refptr<SafeBrowsingUIManager> ui_manager) {
  content::WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);
  // Don't delay the interstitial for prerender pages.
  if (!web_contents ||
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents)) {
    ui_manager->StartDisplayingBlockingPage(resource);
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // Don't delay the interstitial for Chrome Custom Tabs.
  auto* tab_android = TabAndroid::FromWebContents(web_contents);
  if (tab_android && tab_android->IsCustomTab()) {
    ui_manager->StartDisplayingBlockingPage(resource);
    return;
  }
#endif
  SafeBrowsingUserInteractionObserver::CreateForWebContents(
      web_contents, resource, ui_manager);
}

}  // namespace

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      threat_types_(CreateSBThreatTypeSet({
// TODO(crbug.com/41385006): Enable on Android when list is available.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
          safe_browsing::SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE,
#endif
          safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
          safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
          safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
          safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING})) {
}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyNoStatePrefetchContents(
    content::WebContents::OnceGetter web_contents_getter) {
  // Destroy the prefetch with FINAL_STATUS_SAFE_BROWSING.
  // Keep a post task here to avoid possible reentrancy into safe browsing
  // code if it is running on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DestroyNoStatePrefetchContents,
                                std::move(web_contents_getter)));
}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool has_user_gesture) {
  // Keep a post task here to avoid possible reentrancy into safe browsing
  // code if it is running on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SafeBrowsingUIManager::StartDisplayingBlockingPage,
                     ui_manager_, resource));
}

// Starts displaying the SafeBrowsing interstitial page.
void UrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource) {
  // Keep a post task here to avoid possible reentrancy into safe browsing
  // code if it is running on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CreateSafeBrowsingUserInteractionObserver,
                                resource, ui_manager_));
}

bool UrlCheckerDelegateImpl::IsUrlAllowlisted(const GURL& url) {
  return false;
}

void UrlCheckerDelegateImpl::SetPolicyAllowlistDomains(
    const std::vector<std::string>& allowlist_domains) {
  allowlist_domains_ = allowlist_domains;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    base::optional_ref<const base::UnguessableToken> render_frame_token,
    bool originated_from_service_worker) {
  // Check for whether the URL matches the Safe Browsing allowlist domains
  // (a.k. a prefs::kSafeBrowsingAllowlistDomains).
  return base::ranges::any_of(allowlist_domains_,
                              [&original_url](const std::string& domain) {
                                return original_url.DomainIs(domain);
                              });
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  // Keep a post task here to avoid possible reentrancy into safe browsing
  // code if it is running on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NotifySuspiciousSiteTriggerDetected,
                                web_contents_getter));
}

void UrlCheckerDelegateImpl::SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report,
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  ui_manager_->SendThreatDetails(web_contents_getter.Run()->GetBrowserContext(),
                                 std::move(report));
}

bool UrlCheckerDelegateImpl::AreBackgroundHashRealTimeSampleLookupsAllowed(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_getter.Run()->GetBrowserContext());
  return safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs());
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
