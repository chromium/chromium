// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_url_checker_delegate_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/network_service/aw_web_resource_request.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_allowlist_manager.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "android_webview/browser_jni_headers/AwSafeBrowsingConfigHelper_jni.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/security_interstitials/core/urls.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/page_transition_types.h"

namespace android_webview {

namespace {
void CallOnReceivedError(AwContentsClientBridge* client,
                         AwWebResourceRequest request,
                         content::NavigationEntry* entry) {
  if (!client)
    return;
  // We have no way of telling if a navigation was renderer initiated if entry
  // is null but OnReceivedError requires the value to be set, we default to
  // false for that case.
  request.is_renderer_initiated =
      entry ? ui::PageTransitionIsWebTriggerable(entry->GetTransitionType())
            : false;

  // We use ERR_ABORTED here since this is only used in cases where no
  // interstitial is shown.
  client->OnReceivedError(request, net::ERR_ABORTED, true, false);
}
}  // namespace

AwUrlCheckerDelegateImpl::AwUrlCheckerDelegateImpl(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingAllowlistManager* allowlist_manager)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
           safe_browsing::SB_THREAT_TYPE_BILLING})),
      allowlist_manager_(allowlist_manager) {}

AwUrlCheckerDelegateImpl::~AwUrlCheckerDelegateImpl() = default;

void AwUrlCheckerDelegateImpl::MaybeDestroyNoStatePrefetchContents(
    content::WebContents::OnceGetter web_contents_getter) {}

void AwUrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_outermost_main_frame,
    bool has_user_gesture) {
  AwWebResourceRequest request(resource.url.spec(), method,
                               is_outermost_main_frame, has_user_gesture,
                               headers);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AwUrlCheckerDelegateImpl::StartApplicationResponse,
                     ui_manager_, resource, std::move(request)));
}

void AwUrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource,
        bool is_main_frame) {
  NOTREACHED() << "Delayed warnings not implemented for WebView";
}

bool AwUrlCheckerDelegateImpl::IsUrlAllowlisted(const GURL& url) {
  return allowlist_manager_->IsUrlAllowed(url);
}

void AwUrlCheckerDelegateImpl::SetPolicyAllowlistDomains(
    const std::vector<std::string>& allowlist_domains) {
  // The SafeBrowsingAllowlistDomains policy is not supported on AW.
  return;
}

bool AwUrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
  const content::GlobalRenderFrameHostId rfh_id(render_process_id,
                                                render_frame_id);

  std::unique_ptr<AwContentsIoThreadClient> client;
  if (originated_from_service_worker) {
    client = AwContentsIoThreadClient::GetServiceWorkerIoThreadClient();
  } else if (!rfh_id) {
    client = AwContentsIoThreadClient::FromID(frame_tree_node_id);
  } else {
    client = AwContentsIoThreadClient::FromID(rfh_id);
  }

  // If Safe Browsing is disabled by the app, skip the check. Default to
  // performing the check if we can't find the |client|, since the |client| may
  // be null for some service worker requests (see https://crbug.com/979321).
  bool safe_browsing_enabled = client ? client->GetSafeBrowsingEnabled() : true;
  if (!safe_browsing_enabled)
    return true;

  // If this is a hardcoded WebUI URL we use for testing, do not skip the safe
  // browsing check. We do not check user consent here because we do not ever
  // send such URLs to GMS anyway. It's important to ignore user consent in this
  // case because the GMS APIs we rely on to check user consent often get
  // confused during CTS tests, reporting the user has not consented regardless
  // of the on-device setting. See https://crbug.com/938538.
  bool is_hardcoded_url =
      original_url.SchemeIs(content::kChromeUIScheme) &&
      original_url.host() == safe_browsing::kChromeUISafeBrowsingHost;
  if (is_hardcoded_url)
    return false;

  // For other requests, follow user consent.
  JNIEnv* env = base::android::AttachCurrentThread();
  bool safe_browsing_user_consent =
      Java_AwSafeBrowsingConfigHelper_getSafeBrowsingUserOptIn(env);
  return !safe_browsing_user_consent;
}

void AwUrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {}

const safe_browsing::SBThreatTypeSet&
AwUrlCheckerDelegateImpl::GetThreatTypes() {
  return threat_types_;
}

safe_browsing::SafeBrowsingDatabaseManager*
AwUrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

safe_browsing::BaseUIManager* AwUrlCheckerDelegateImpl::GetUIManager() {
  return ui_manager_.get();
}

// static
void AwUrlCheckerDelegateImpl::StartApplicationResponse(
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource,
    const AwWebResourceRequest& request) {
  content::WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);

  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(web_contents);
  if (ui_manager->IsAllowlisted(resource) && security_interstitial_tab_helper &&
      security_interstitial_tab_helper->IsDisplayingInterstitial()) {
    // In this case we are about to leave an interstitial due to the user
    // clicking proceed on it, we shouldn't call OnSafeBrowsingHit again.
    resource.callback_sequence->PostTask(
        FROM_HERE, base::BindOnce(resource.callback, true /* proceed */,
                                  false /* showed_interstitial */));
    return;
  }

  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);

  if (client) {
    base::OnceCallback<void(SafeBrowsingAction, bool)> callback =
        base::BindOnce(&AwUrlCheckerDelegateImpl::DoApplicationResponse,
                       ui_manager, resource, request);

    client->OnSafeBrowsingHit(request, resource.threat_type,
                              std::move(callback));
  }
}

// static
void AwUrlCheckerDelegateImpl::DoApplicationResponse(
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource,
    const AwWebResourceRequest& request,
    SafeBrowsingAction action,
    bool reporting) {
  content::WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);
  // |web_contents| can be null after RenderFrameHost is destroyed.
  if (!web_contents)
    return;

  if (!reporting) {
    AwBrowserContext* browser_context =
        AwBrowserContext::FromWebContents(web_contents);
    browser_context->SetExtendedReportingAllowed(false);
  }

  content::NavigationEntry* entry = GetNavigationEntryForResource(resource);

  // TODO(ntfschr): fully handle reporting once we add support (crbug/688629)
  bool proceed;
  switch (action) {
    case SafeBrowsingAction::SHOW_INTERSTITIAL: {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &AwUrlCheckerDelegateImpl::StartDisplayingDefaultBlockingPage,
              ui_manager, resource));
      AwContents* contents = AwContents::FromWebContents(web_contents);
      if (!contents || !contents->CanShowInterstitial()) {
        // With committed interstitials OnReceivedError for safe browsing
        // blocked sites is generally called from the blocking page object.
        // When CanShowInterstitial is false, no blocking page is created from
        // StartDisplayingDefaultBlockingPage, so we handle the call here.
        CallOnReceivedError(
            AwContentsClientBridge::FromWebContents(web_contents), request,
            entry);
      }
    }
      return;
    case SafeBrowsingAction::PROCEED:
      proceed = true;
      break;
    case SafeBrowsingAction::BACK_TO_SAFETY:
      proceed = false;
      break;
    default:
      NOTREACHED();
  }

  if (!proceed) {
    // With committed interstitials OnReceivedError for safe browsing blocked
    // sites is generally called from the blocking page object. Since no
    // blocking page is created in this case, we manually call it here.
    CallOnReceivedError(AwContentsClientBridge::FromWebContents(web_contents),
                        request, entry);
  }

  // Navigate back for back-to-safety on subresources
  if (!proceed && resource.is_subframe) {
    if (web_contents->GetController().CanGoBack()) {
      web_contents->GetController().GoBack();
    } else {
      web_contents->GetController().LoadURL(
          ui_manager->default_safe_page(), content::Referrer(),
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
    }
  }

  GURL main_frame_url = entry ? entry->GetURL() : GURL();
  ui_manager->OnBlockingPageDone(
      std::vector<security_interstitials::UnsafeResource>{resource}, proceed,
      web_contents, main_frame_url, false /* showed_interstitial */);
}

// static
void AwUrlCheckerDelegateImpl::StartDisplayingDefaultBlockingPage(
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);
  if (web_contents) {
    ui_manager->DisplayBlockingPage(resource);
    return;
  }

  // Reporting back that it is not okay to proceed with loading the URL.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(resource.callback, false /* proceed */,
                                false /* showed_interstitial */));
}

void AwUrlCheckerDelegateImpl::CheckLookupMechanismExperimentEligibility(
    const security_interstitials::UnsafeResource& resource,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  NOTREACHED();
}
void AwUrlCheckerDelegateImpl::CheckExperimentEligibilityAndStartBlockingPage(
    const security_interstitials::UnsafeResource& resource,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  NOTREACHED();
}

}  // namespace android_webview
