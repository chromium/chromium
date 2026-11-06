// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include "base/barrier_closure.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_factory.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"
#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service_factory.h"
#include "chrome/browser/safe_browsing/interstitials/user_interaction_observer.h"
#include "chrome/browser/safe_browsing/navigation_observer/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/navigation_observer/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#endif

namespace safe_browsing {

namespace {
// The number of user gestures we trace back for CSD attribution.
const int kCSDAttributionUserGestureLimitForExtendedReporting = 5;

// Returns true if `rfh` is a cross-origin or out-of-process subframe relative
// to `parent`. Same-origin in-process `<iframe>`s are omitted here because
// Blink's `content_extraction::GetInnerText()` already extracts them inline.
bool ShouldExtractSubframeSeparately(content::RenderFrameHost* rfh,
                                     content::RenderFrameHost* parent) {
  // Blink's `InnerTextBuilder` only inlines `HTMLIFrameElement` (`kIframe`),
  // not `<frame>`, `<object>`, or `<embed>`.
  return rfh->GetFrameOwnerElementType() !=
             blink::FrameOwnerElementType::kIframe ||
         rfh->GetRenderWidgetHost() != parent->GetRenderWidgetHost() ||
         !rfh->GetLastCommittedOrigin().IsSameOriginWith(
             parent->GetLastCommittedOrigin());
}

void OnAllFramesInnerTextDone(
    content::WeakDocumentPtr main_document,
    scoped_refptr<base::RefCountedData<std::vector<std::string>>> frame_texts,
    scoped_refptr<base::RefCountedData<HostInnerTextCallback>> callback_holder,
    base::TimeTicks start_time,
    bool has_loading_subframe,
    bool timed_out) {
  if (!callback_holder->data) {
    return;
  }
  HostInnerTextCallback callback = std::move(callback_holder->data);
  if (!main_document.AsRenderFrameHostIfValid()) {
    std::move(callback).Run("");
    return;
  }

  base::UmaHistogramTimes(
      "SBClientPhishing.IntelligentScanGetInnerTextDuration",
      base::TimeTicks::Now() - start_time);

  const std::vector<std::string>& texts = frame_texts->data;
  base::UmaHistogramCounts100("SBClientPhishing.IntelligentScanIframeCount",
                              texts.size() - 1);

  if (has_loading_subframe || timed_out) {
    const bool is_main_frame_empty =
        base::TrimWhitespaceASCII(texts[0], base::TRIM_ALL).empty();
    base::UmaHistogramBoolean(
        "SBClientPhishing.IntelligentScanEmptyMainFrameWithLoadingIframes",
        is_main_frame_empty);
  }

  size_t iframe_inner_text_bytes = 0;
  std::vector<std::string_view> non_empty_parts;
  non_empty_parts.reserve(texts.size());
  for (size_t i = 0; i < texts.size(); ++i) {
    if (!base::TrimWhitespaceASCII(texts[i], base::TRIM_ALL).empty()) {
      if (i > 0) {
        iframe_inner_text_bytes += texts[i].size();
      }
      non_empty_parts.push_back(texts[i]);
    }
  }

  if (texts.size() > 1) {
    base::UmaHistogramCounts100000(
        "SBClientPhishing.IntelligentScanIframeInnerTextSize",
        iframe_inner_text_bytes);
  }

  std::move(callback).Run(base::JoinString(non_empty_parts, "\n"));
}

}  // namespace

// static
std::unique_ptr<ClientSideDetectionHost>
ChromeClientSideDetectionHostDelegate::CreateHost(content::WebContents* tab) {
  content::BrowserContext* browser_context = tab->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(profile);
  return ClientSideDetectionHost::Create(
      tab, std::make_unique<ChromeClientSideDetectionHostDelegate>(tab),
      ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(profile),
      profile->GetPrefs(), VerdictCacheManagerFactory::GetForProfile(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS),
      csd_service ? csd_service->GetWeakPtr() : nullptr,
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile)),
      profile->IsOffTheRecord(),
      base::BindRepeating(&safe_browsing::SyncUtils::IsPrimaryAccountSignedIn,
                          IdentityManagerFactory::GetForProfile(profile)));
}

ChromeClientSideDetectionHostDelegate::ChromeClientSideDetectionHostDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}
ChromeClientSideDetectionHostDelegate::
    ~ChromeClientSideDetectionHostDelegate() = default;

bool ChromeClientSideDetectionHostDelegate::
    HasSafeBrowsingUserInteractionObserver() {
  return SafeBrowsingUserInteractionObserver::FromWebContents(web_contents_);
}

scoped_refptr<SafeBrowsingDatabaseManager>
ChromeClientSideDetectionHostDelegate::GetSafeBrowsingDBManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->database_manager().get() : nullptr;
}

SafeBrowsingNavigationObserverManager* ChromeClientSideDetectionHostDelegate::
    GetSafeBrowsingNavigationObserverManager() {
  if (observer_manager_for_testing_) {
    return observer_manager_for_testing_;
  }

  return SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

scoped_refptr<BaseUIManager>
ChromeClientSideDetectionHostDelegate::GetSafeBrowsingUIManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->ui_manager() : nullptr;
}

void ChromeClientSideDetectionHostDelegate::AddReferrerChain(
    ClientPhishingRequest* verdict,
    GURL current_url,
    const content::GlobalRenderFrameHostId& current_outermost_main_frame_id) {
  SafeBrowsingNavigationObserverManager* navigation_observer_manager =
      GetSafeBrowsingNavigationObserverManager();
  if (!navigation_observer_manager) {
    return;
  }
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager->IdentifyReferrerChainByEventURL(
          current_url, SessionID::InvalidValue(),
          current_outermost_main_frame_id,
          kCSDAttributionUserGestureLimitForExtendedReporting,
          verdict->mutable_referrer_chain());

  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.ReferrerURLChainSize.CSDAttribution",
                           verdict->referrer_chain().size());
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.CSDAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  // Determines how many recent navigation events to append to referrer
  // chain if any.
  size_t recent_navigations_to_collect =
      CountOfRecentNavigationsToAppend(result);

  navigation_observer_manager->AppendRecentNavigations(
      recent_navigations_to_collect, verdict->mutable_referrer_chain());
}

size_t ChromeClientSideDetectionHostDelegate::CountOfRecentNavigationsToAppend(
    SafeBrowsingNavigationObserverManager::AttributionResult result) {
  auto* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return web_contents_ ? SafeBrowsingNavigationObserverManager::
                             CountOfRecentNavigationsToAppend(
                                 profile, profile->GetPrefs(), result)
                       : 0u;
}

ChromeUserPopulation
ChromeClientSideDetectionHostDelegate::GetUserPopulation() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return ::safe_browsing::GetUserPopulationForProfile(profile);
}

void ChromeClientSideDetectionHostDelegate::GetInnerText(
    HostInnerTextCallback callback) {
  content::RenderFrameHost* primary_main_frame =
      web_contents_->GetPrimaryMainFrame();
  if (!base::FeatureList::IsEnabled(kClientSideDetectionIframeInnerText)) {
    content_extraction::GetInnerText(
        *primary_main_frame, std::nullopt,
        base::BindOnce(
            &ChromeClientSideDetectionHostDelegate::OnInnerTextResult,
            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  const size_t max_iframes =
      std::max(0, kClientSideDetectionMaxIframesToExtract.Get());
  std::vector<content::RenderFrameHost*> frames_to_query;
  bool has_loading_subframe = false;
  primary_main_frame->ForEachRenderFrameHostWithAction(
      [primary_main_frame, max_iframes, &frames_to_query,
       &has_loading_subframe](content::RenderFrameHost* rfh) {
        if (rfh == primary_main_frame) {
          // CHECK we don't break an invariant: Downstream code assumes the
          // first item in frames_to_query is always the primary_main_frame.
          CHECK(frames_to_query.empty());
          frames_to_query.push_back(rfh);
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        }
        content::RenderFrameHost* parent = rfh->GetParent();
        // Skip fenced frame trees and inner frame trees (`!parent`), inactive
        // or non-live frames, and ad-tagged subframes (consistent with Blink's
        // `ShouldContentExtractionIncludeIFrame()`).
        if (!parent || rfh->IsAdFrame() ||
            rfh->GetLifecycleState() !=
                content::RenderFrameHost::LifecycleState::kActive ||
            !rfh->IsRenderFrameLive()) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        // Skip non-web subframes such as `chrome-extension://` iframes.
        const GURL precursor = rfh->GetLastCommittedOrigin()
                                   .GetTupleOrPrecursorTupleIfOpaque()
                                   .GetURL();
        if (!precursor.SchemeIsHTTPOrHTTPS()) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        if (!rfh->IsDOMContentLoaded() ||
            rfh->GetLastCommittedURL().is_empty()) {
          has_loading_subframe = true;
        }
        if (ShouldExtractSubframeSeparately(rfh, parent) &&
            frames_to_query.size() - 1 < max_iframes) {
          frames_to_query.push_back(rfh);
        }
        // Always continue into child frames so cross-origin grandchildren
        // nested inside a same-origin iframe are still visited and extracted.
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  auto frame_texts =
      base::MakeRefCounted<base::RefCountedData<std::vector<std::string>>>(
          std::in_place, frames_to_query.size());
  auto callback_holder =
      base::MakeRefCounted<base::RefCountedData<HostInnerTextCallback>>(
          std::move(callback));
  auto complete_once = base::BindRepeating(
      &OnAllFramesInnerTextDone, primary_main_frame->GetWeakDocumentPtr(),
      frame_texts, callback_holder, base::TimeTicks::Now(),
      has_loading_subframe);

  base::RepeatingClosure barrier =
      base::BarrierClosure(frames_to_query.size(),
                           base::BindOnce(complete_once, /*timed_out=*/false));
  for (size_t i = 0; i < frames_to_query.size(); ++i) {
    content_extraction::GetInnerText(
        *frames_to_query[i], std::nullopt,
        base::BindOnce(
            [](scoped_refptr<base::RefCountedData<std::vector<std::string>>>
                   texts,
               base::OnceClosure timeout_closure, size_t index,
               base::OnceClosure done,
               std::unique_ptr<content_extraction::InnerTextResult> result) {
              if (result) {
                texts->data[index] = std::move(result->inner_text);
              }
              std::move(done).Run();
              // Start the subframe timeout once the main frame (index 0) has
              // responded. It is a no-op if all frames have already responded.
              if (index == 0) {
                base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                    FROM_HERE, std::move(timeout_closure),
                    kClientSideDetectionIframeInnerTextTimeout.Get());
              }
            },
            frame_texts, base::BindOnce(complete_once, /*timed_out=*/true), i,
            barrier));
  }
}

void ChromeClientSideDetectionHostDelegate::MaybeStartGeminiAntiscamProtection(
    GURL url,
    ClientSideDetectionType request_type,
    std::optional<bool> did_match_high_confidence_allowlist) {
  GeminiAntiscamProtectionService* service =
      GeminiAntiscamProtectionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (service) {
    // GetInnerText is called with a callback that will call
    // GeminiAntiscamProtectionService::MaybeStartAntiscamProtection.
    // The `did_match_high_confidence_allowlist` value is true if not set so
    // that it will not trigger Gemini Antiscam Protection.
    GetInnerText(base::BindOnce(
        &GeminiAntiscamProtectionService::MaybeStartAntiscamProtection,
        service->GetWeakPtr(),
        GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
            web_contents_),
        url, request_type, did_match_high_confidence_allowlist.value_or(true),
        web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL()));
  }
}

void ChromeClientSideDetectionHostDelegate::OnInnerTextResult(
    HostInnerTextCallback callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  std::move(callback).Run(result ? result->inner_text : "");
}

#if BUILDFLAG(IS_ANDROID)
internal::ReferringAppInfo
ChromeClientSideDetectionHostDelegate::GetReferringAppInfo(
    content::WebContents* web_contents) {
  return safe_browsing::GetReferringAppInfo(web_contents,
                                            /*get_webapk_info=*/false);
}
#endif

}  // namespace safe_browsing
