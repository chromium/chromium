// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_content_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"
#include "chrome/browser/previews/previews_offline_helper.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace previews {

bool IsPrivateDomain(const GURL& url) {
  if (url.host().find(".") == base::StringPiece::npos)
    return true;

  // Allow localhost check to be skipped if needed, like in testing.
  if (net::IsLocalhost(url))
    return !previews::params::LitePagePreviewsTriggerOnLocalhost();

  net::IPAddress ip_addr;
  if (url.HostIsIPAddress() && ip_addr.AssignFromIPLiteral(url.host()) &&
      !ip_addr.IsPubliclyRoutable()) {
    return true;
  }
  return false;
}

bool ShouldAllowRedirectPreview(content::NavigationHandle* navigation_handle) {
  // This should only occur in unit tests, this behavior is tested in browser
  // tests.
  if (!navigation_handle)
    return true;

  // This should only occur in unit tests; this behavior is tested in browser
  // tests.
  if (navigation_handle->GetWebContents() == nullptr)
    return true;

  const GURL& url = navigation_handle->GetURL();
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  auto* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  PreviewsLitePageRedirectDecider* decider =
      previews_service->previews_lite_page_redirect_decider();

  std::vector<previews::LitePageRedirectIneligibleReason> ineligible_reasons;

  if (!url.SchemeIs(url::kHttpsScheme)) {
    ineligible_reasons.push_back(
        previews::LitePageRedirectIneligibleReason::kNonHttpsScheme);
  }

  if (decider->IsServerUnavailable()) {
    ineligible_reasons.push_back(
        previews::LitePageRedirectIneligibleReason::kServerUnavailable);
  }

  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))
          .get();
  ContentSetting setting;
  GURL previews_url = GetLitePageRedirectURLForURL(url);
  cookie_settings->GetCookieSetting(previews_url, previews_url, nullptr,
                                    &setting);
  if (!content_settings::CookieSettingsBase::IsAllowed(setting)) {
    ineligible_reasons.push_back(
        previews::LitePageRedirectIneligibleReason::kCookiesBlocked);
  }

  if (!decider->has_drp_headers()) {
    ineligible_reasons.push_back(
        previews::LitePageRedirectIneligibleReason::kInvalidProxyHeaders);
  }

  if (previews::params::LitePageRedirectOnlyTriggerOnSuccessfulProbe()) {
    if (!decider->IsServerProbeResultAvailable()) {
      ineligible_reasons.push_back(
          previews::LitePageRedirectIneligibleReason::kServiceProbeIncomplete);
    } else if (!decider->IsServerReachableByProbe()) {
      ineligible_reasons.push_back(
          previews::LitePageRedirectIneligibleReason::kServiceProbeFailed);
    }
  }

  if (!previews::params::LitePageRedirectTriggerOnAPITransition() &&
      navigation_handle->GetPageTransition() & ui::PAGE_TRANSITION_FROM_API) {
    ineligible_reasons.push_back(
        previews::LitePageRedirectIneligibleReason::kAPIPageTransition);
  }

  if (previews::params::LitePageRedirectValidateForwardBackTransition() &&
      navigation_handle->GetPageTransition() &
          ui::PAGE_TRANSITION_FORWARD_BACK) {
    // On forward/back navigations, enable LPR iff we showed an LPR for the
    // prior navigation that we are returning to. For example, suppose the user
    // navigates to A then B, without getting a preview for either navigation.
    // If the user navigates back to A, we should load the original page as that
    // is likely cached; loading a preview would be much slower as that requires
    // contacting the litepages server.
    if (!previews::IsLitePageRedirectPreviewURL(navigation_handle->GetURL())) {
      ineligible_reasons.push_back(previews::LitePageRedirectIneligibleReason::
                                       kForwardBackPageTransition);
    }
  }

  // Record UMA.
  for (previews::LitePageRedirectIneligibleReason reason : ineligible_reasons) {
    previews::LogLitePageRedirectIneligibleReason(reason);
  }
  if (!ineligible_reasons.empty())
    return false;

  // Check dynamic blacklists.
  std::vector<previews::LitePageRedirectBlacklistReason> blacklist_reasons;

  if (IsPrivateDomain(url)) {
    blacklist_reasons.push_back(
        previews::LitePageRedirectBlacklistReason::kNavigationToPrivateDomain);
  }

  if (decider->HostBlacklistedFromBypass(url.host())) {
    blacklist_reasons.push_back(
        previews::LitePageRedirectBlacklistReason::kHostBypassBlacklisted);
  }

  // Record UMA
  for (previews::LitePageRedirectBlacklistReason reason : blacklist_reasons) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.BlacklistReasons",
                              reason);
  }

  if (!blacklist_reasons.empty())
    return false;

  // This should always be at the end, but before the control group check.
  if (decider->NeedsToNotifyUser()) {
    previews::LogLitePageRedirectIneligibleReason(
        previews::LitePageRedirectIneligibleReason::kInfoBarNotSeen);
    return false;
  }

  // This should always be last.
  if (previews::params::IsInLitePageRedirectControl()) {
    previews::PreviewsUserData::ServerLitePageInfo* info =
        PreviewsLitePageRedirectURLLoaderInterceptor::
            GetOrCreateServerLitePageInfo(navigation_handle, decider);
    info->status = previews::ServerLitePageStatus::kControl;
    return false;
  }

  return true;
}

std::unique_ptr<PreviewsUserData::ServerLitePageInfo>
CreateServerLitePageInfoFromNavigationHandle(
    content::NavigationHandle* navigation_handle) {
  auto server_lite_page_info =
      std::make_unique<PreviewsUserData::ServerLitePageInfo>();
  // This is only for unit testing.
  if (!navigation_handle->GetWebContents())
    return nullptr;

  server_lite_page_info->original_navigation_start =
      navigation_handle->NavigationStart();

  const net::HttpRequestHeaders& headers =
      navigation_handle->GetRequestHeaders();

  const ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<const ChromeNavigationUIData*>(
          navigation_handle->GetNavigationUIData());
  server_lite_page_info->page_id =
      chrome_navigation_ui_data->data_reduction_proxy_page_id();

  base::Optional<std::string> session_key =
      data_reduction_proxy::DataReductionProxyRequestOptions::
          GetSessionKeyFromRequestHeaders(headers);
  if (!session_key) {
    // This is a less-authoritative source to get the session key from since
    // there is a very small chance that the proxy config has been updated
    // between sending the original request and now. However, since this is only
    // for metrics purposes, it is better than not having any session key.
    DataReductionProxyChromeSettings* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            navigation_handle->GetWebContents()->GetBrowserContext());
    session_key = data_reduction_proxy::DataReductionProxyRequestOptions::
        GetSessionKeyFromRequestHeaders(drp_settings->GetProxyRequestHeaders());
  }
  if (session_key)
    server_lite_page_info->drp_session_key = session_key.value();
  DCHECK_NE(server_lite_page_info->drp_session_key, "");

  server_lite_page_info->status = ServerLitePageStatus::kSuccess;
  server_lite_page_info->restart_count = 0;
  return server_lite_page_info;
}

bool HasEnabledPreviews(content::PreviewsState previews_state) {
  return previews_state != content::PREVIEWS_UNSPECIFIED &&
         !(previews_state & content::PREVIEWS_OFF) &&
         !(previews_state & content::PREVIEWS_NO_TRANSFORM);
}

content::PreviewsState DetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    bool previews_triggering_logic_already_ran,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle) {
  content::PreviewsState previews_state = content::PREVIEWS_UNSPECIFIED;

  const GURL& url = navigation_handle->GetURL();
  bool is_reload =
      navigation_handle->GetReloadType() != content::ReloadType::NONE;

  // Either this is a navigation to the lite page via the redirect mechanism and
  // only Lite Page redirect should be served, or this is a reload in which case
  // the Lite Page mechanism should redirect to the original URL. Either way,
  // set the allowed PreviewsState.
  if (IsLitePageRedirectPreviewURL(url)) {
    return content::LITE_PAGE_REDIRECT_ON;
  }

  if (!previews::params::ArePreviewsAllowed()) {
    return previews_state;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return previews_state;
  }

  if (!is_data_saver_user)
    return previews_state;

  auto* previews_service =
      navigation_handle && navigation_handle->GetWebContents()
          ? PreviewsServiceFactory::GetForProfile(Profile::FromBrowserContext(
                navigation_handle->GetWebContents()->GetBrowserContext()))
          : nullptr;

  if (previews_triggering_logic_already_ran) {
    // Record that the navigation was redirected.
    previews_data->set_is_redirect(true);
  }

  bool allow_offline = true;
  // If |previews_service| is null, skip the previews offline helper check.
  // This only happens in testing.
  if (previews_service) {
    allow_offline = previews_service->previews_offline_helper()
                        ->ShouldAttemptOfflinePreview(url);
  }
  allow_offline =
      allow_offline && previews_decider->ShouldAllowPreviewAtNavigationStart(
                           previews_data, navigation_handle, is_reload,
                           previews::PreviewsType::OFFLINE);

  if (allow_offline)
    previews_state |= content::OFFLINE_PAGE_ON;

  // Check PageHint preview types first.
  bool should_load_page_hints = false;
  if (previews_decider->ShouldAllowPreviewAtNavigationStart(
          previews_data, navigation_handle, is_reload,
          previews::PreviewsType::DEFER_ALL_SCRIPT)) {
    previews_state |= content::DEFER_ALL_SCRIPT_ON;
    should_load_page_hints = true;
  }
  if (previews_decider->ShouldAllowPreviewAtNavigationStart(
          previews_data, navigation_handle, is_reload,
          previews::PreviewsType::RESOURCE_LOADING_HINTS)) {
    previews_state |= content::RESOURCE_LOADING_HINTS_ON;
    should_load_page_hints = true;
  }
  if (previews_decider->ShouldAllowPreviewAtNavigationStart(
          previews_data, navigation_handle, is_reload,
          previews::PreviewsType::NOSCRIPT)) {
    previews_state |= content::NOSCRIPT_ON;
    should_load_page_hints = true;
  }
  bool has_page_hints = false;
  if (should_load_page_hints) {
    // Initiate load of any applicable page hint details.
    has_page_hints = previews_decider->LoadPageHints(navigation_handle);
  }

  if ((!has_page_hints || params::LitePagePreviewsOverridePageHints()) &&
      previews_decider->ShouldAllowPreviewAtNavigationStart(
          previews_data, navigation_handle, is_reload,
          previews::PreviewsType::LITE_PAGE_REDIRECT) &&
      ShouldAllowRedirectPreview(navigation_handle)) {
    previews_state |= content::LITE_PAGE_REDIRECT_ON;
  }

  return previews_state;
}

content::PreviewsState DetermineCommittedServerPreviewsState(
    data_reduction_proxy::DataReductionProxyData* data,
    content::PreviewsState initial_state) {
  if (!data) {
    return initial_state &= ~(content::SERVER_LITE_PAGE_ON);
  }
  content::PreviewsState updated_state = initial_state;
  if (!data->lite_page_received()) {
    // Turn off LitePage bit.
    updated_state &= ~(content::SERVER_LITE_PAGE_ON);
  }
  return updated_state;
}

void LogCommittedPreview(previews::PreviewsUserData* previews_data,
                         PreviewsType type) {
  net::EffectiveConnectionType navigation_ect = previews_data->navigation_ect();
  UMA_HISTOGRAM_ENUMERATION("Previews.Triggered.EffectiveConnectionType2",
                            navigation_ect,
                            net::EFFECTIVE_CONNECTION_TYPE_LAST);
  base::UmaHistogramEnumeration(
      base::StringPrintf("Previews.Triggered.EffectiveConnectionType2.%s",
                         GetStringNameForType(type).c_str()),
      navigation_ect, net::EFFECTIVE_CONNECTION_TYPE_LAST);
}

// Records the result of the coin flip in PreviewsUserData and UKM. This may be
// called multiple times during a navigation (like on redirects), but once
// called with one |result|, that value is not expected to change.
void UpdatePreviewsUserDataAndRecordCoinFlipResult(
    content::NavigationHandle* navigation_handle,
    previews::PreviewsUserData* previews_user_data,
    previews::CoinFlipHoldbackResult result) {
  DCHECK_NE(result, previews::CoinFlipHoldbackResult::kNotSet);

  // Log a coin flip holdback to the interventions-internals page.
  if (result == previews::CoinFlipHoldbackResult::kHoldback) {
    auto* previews_service =
        navigation_handle && navigation_handle->GetWebContents()
            ? PreviewsServiceFactory::GetForProfile(Profile::FromBrowserContext(
                  navigation_handle->GetWebContents()->GetBrowserContext()))
            : nullptr;
    if (previews_service && previews_service->previews_ui_service()) {
      PreviewsEligibilityReason reason =
          PreviewsEligibilityReason::COINFLIP_HOLDBACK;
      PreviewsType type =
          previews_user_data->PreHoldbackCommittedPreviewsType();
      std::vector<PreviewsEligibilityReason> passed_reasons;
      previews_service->previews_ui_service()->LogPreviewDecisionMade(
          reason, navigation_handle->GetURL(),
          base::DefaultClock::GetInstance()->Now(), type,
          std::move(passed_reasons), previews_user_data->page_id());
    }
  }

  // We only want to record the coin flip once per navigation when set, so if it
  // is already set then we're done.
  if (previews_user_data->coin_flip_holdback_result() !=
      previews::CoinFlipHoldbackResult::kNotSet) {
    // The coin flip result value should never change its set state during a
    // navigation.
    DCHECK_EQ(previews_user_data->coin_flip_holdback_result(), result);
    return;
  }

  previews_user_data->set_coin_flip_holdback_result(result);

  ukm::builders::PreviewsCoinFlip builder(ukm::ConvertToSourceId(
      navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID));
  builder.Setcoin_flip_result(static_cast<int>(result));
  builder.Record(ukm::UkmRecorder::Get());
}

content::PreviewsState DetermineCommittedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    content::PreviewsState previews_state,
    const previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle) {
  // Check if an offline preview was actually served.
  if (previews_data && previews_data->offline_preview_used()) {
    DCHECK(previews_state & content::OFFLINE_PAGE_ON);
    LogCommittedPreview(previews_data, PreviewsType::OFFLINE);
    return content::OFFLINE_PAGE_ON;
  }
  previews_state &= ~content::OFFLINE_PAGE_ON;

  // If a server preview is set, retain only the bits determined for the server.
  // |previews_state| must already have been updated for server previews from
  // the main frame response headers (so if they are set here, then they are
  // the specify the committed preview).
  if (previews_state & content::SERVER_LITE_PAGE_ON) {
    LogCommittedPreview(previews_data, PreviewsType::LITE_PAGE);
    return previews_state & content::SERVER_LITE_PAGE_ON;
  }

  if (previews_data && previews_data->cache_control_no_transform_directive()) {
    if (HasEnabledPreviews(previews_state)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.CacheControlNoTransform.BlockedPreview",
          GetMainFramePreviewsType(previews_state),
          previews::PreviewsType::LAST);
    }
    return content::PREVIEWS_OFF;
  }

  // Check if a LITE_PAGE_REDIRECT preview was actually served.
  if (previews_state & content::LITE_PAGE_REDIRECT_ON) {
    if (IsLitePageRedirectPreviewURL(url)) {
      if (navigation_handle) {
        previews_data->set_server_lite_page_info(
            CreateServerLitePageInfoFromNavigationHandle(navigation_handle));
      }
      LogCommittedPreview(previews_data, PreviewsType::LITE_PAGE_REDIRECT);
      return content::LITE_PAGE_REDIRECT_ON;
    }
    previews_state &= ~content::LITE_PAGE_REDIRECT_ON;
  }
  DCHECK(!IsLitePageRedirectPreviewURL(url));

  // Check if the URL is eligible for defer all script preview. A URL
  // may not be eligible for the preview if it's likely to cause a
  // client redirect loop.
  if ((previews::params::DetectDeferRedirectLoopsUsingCache()) &&
      (previews_state & content::DEFER_ALL_SCRIPT_ON)) {
    content::WebContents* web_contents =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;
    if (web_contents) {
      auto* previews_service = PreviewsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

      if (previews_service &&
          !previews_service->IsUrlEligibleForDeferAllScriptPreview(url)) {
        previews_state &= ~content::DEFER_ALL_SCRIPT_ON;
        UMA_HISTOGRAM_BOOLEAN(
            "Previews.DeferAllScript.RedirectLoopDetectedUsingCache", true);
        if (previews_service->previews_ui_service()) {
          previews::PreviewsDeciderImpl* previews_decider_impl =
              previews_service->previews_ui_service()->previews_decider_impl();
          DCHECK(previews_decider_impl);
          std::vector<PreviewsEligibilityReason> passed_reasons;
          previews_decider_impl->LogPreviewDecisionMade(
              PreviewsEligibilityReason::REDIRECT_LOOP_DETECTED,
              navigation_handle->GetURL(),
              base::DefaultClock::GetInstance()->Now(),
              PreviewsType::DEFER_ALL_SCRIPT, std::move(passed_reasons),
              previews_data);
        }
      }
    }
  }

  if (previews_state & content::DEFER_ALL_SCRIPT_ON) {
    content::WebContents* web_contents =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;
    if (web_contents) {
      auto* previews_service = PreviewsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

      if (previews_service &&
          previews_service->MatchesDeferAllScriptDenyListRegexp(url)) {
        previews_state &= ~content::DEFER_ALL_SCRIPT_ON;
        UMA_HISTOGRAM_BOOLEAN("Previews.DeferAllScript.DenyListMatch", true);
        if (previews_service->previews_ui_service()) {
          previews::PreviewsDeciderImpl* previews_decider_impl =
              previews_service->previews_ui_service()->previews_decider_impl();
          DCHECK(previews_decider_impl);
          std::vector<PreviewsEligibilityReason> passed_reasons;
          previews_decider_impl->LogPreviewDecisionMade(
              PreviewsEligibilityReason::DENY_LIST_MATCHED,
              navigation_handle->GetURL(),
              base::DefaultClock::GetInstance()->Now(),
              PreviewsType::DEFER_ALL_SCRIPT, std::move(passed_reasons),
              previews_data);
        }
      }
    }
  }

  // Make priority decision among allowed client preview types that can be
  // decided at Commit time.

  if (previews_state & content::DEFER_ALL_SCRIPT_ON) {
    // DeferAllScript was allowed for the original URL but only continue with it
    // if the committed URL has HTTPS scheme and is allowed by decider.
    if (previews_decider && previews_decider->ShouldCommitPreview(
                                previews_data, navigation_handle,
                                previews::PreviewsType::DEFER_ALL_SCRIPT)) {
      LogCommittedPreview(previews_data, PreviewsType::DEFER_ALL_SCRIPT);
      return content::DEFER_ALL_SCRIPT_ON;
    }
    // Remove DEFER_ALL_SCRIPT_ON from |previews_state| since we decided not to
    // commit to it.
    previews_state = previews_state & ~content::DEFER_ALL_SCRIPT_ON;
  }

  if (previews_state & content::RESOURCE_LOADING_HINTS_ON) {
    // Resource loading hints was chosen for the original URL but only continue
    // with it if the committed URL has HTTPS scheme and is allowed by decider.
    if (previews_decider &&
        previews_decider->ShouldCommitPreview(
            previews_data, navigation_handle,
            previews::PreviewsType::RESOURCE_LOADING_HINTS)) {
      LogCommittedPreview(previews_data, PreviewsType::RESOURCE_LOADING_HINTS);
      return content::RESOURCE_LOADING_HINTS_ON;
    }
    // Remove RESOURCE_LOADING_HINTS_ON from |previews_state| since we decided
    // not to commit to it.
    previews_state = previews_state & ~content::RESOURCE_LOADING_HINTS_ON;
  }

  if (previews_state & content::NOSCRIPT_ON) {
    // NoScript was chosen for the original URL but only continue with it
    // if the committed URL has HTTPS scheme and is allowed by decider.
    if (previews_decider && previews_decider->ShouldCommitPreview(
                                previews_data, navigation_handle,
                                previews::PreviewsType::NOSCRIPT)) {
      LogCommittedPreview(previews_data, PreviewsType::NOSCRIPT);
      return content::NOSCRIPT_ON;
    }
    // Remove NOSCRIPT_ON from |previews_state| since we decided not to
    // commit to it.
    previews_state = previews_state & ~content::NOSCRIPT_ON;
  }

  if (!previews_state) {
    return content::PREVIEWS_OFF;
  }

  DCHECK(previews_state == content::PREVIEWS_OFF ||
         previews_state == content::PREVIEWS_UNSPECIFIED);
  return content::PREVIEWS_OFF;
}

content::PreviewsState MaybeCoinFlipHoldbackBeforeCommit(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(features::kCoinFlipHoldback))
    return initial_state;

  // Get PreviewsUserData to store the result of the coin flip. If it can't be
  // gotten, return early.
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  PreviewsUserData* previews_data =
      ui_tab_helper ? ui_tab_helper->GetPreviewsUserData(navigation_handle)
                    : nullptr;
  if (!previews_data)
    return initial_state;

  // It is possible that any number of at-commit-decided previews are enabled,
  // but do not hold them back until the commit time logic runs.
  if (!HasEnabledPreviews(initial_state & kPreCommitPreviews))
    return initial_state;

  if (previews_data->CoinFlipForNavigation()) {
    // Holdback all previews. It is possible that some number of client previews
    // will also be held back here. However, since a before-commit preview was
    // likely, we turn off all of them to make analysis simpler and this code
    // more robust.
    UpdatePreviewsUserDataAndRecordCoinFlipResult(
        navigation_handle, previews_data, CoinFlipHoldbackResult::kHoldback);
    return content::PREVIEWS_OFF;
  }

  UpdatePreviewsUserDataAndRecordCoinFlipResult(
      navigation_handle, previews_data, CoinFlipHoldbackResult::kAllowed);
  return initial_state;
}

content::PreviewsState MaybeCoinFlipHoldbackAfterCommit(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(features::kCoinFlipHoldback))
    return initial_state;

  // Get PreviewsUserData to store the result of the coin flip. If it can't be
  // gotten, return early.
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  PreviewsUserData* previews_data =
      ui_tab_helper ? ui_tab_helper->GetPreviewsUserData(navigation_handle)
                    : nullptr;
  if (!previews_data)
    return initial_state;

  if (!HasEnabledPreviews(initial_state))
    return initial_state;

  if (previews_data->CoinFlipForNavigation()) {
    // No pre-commit previews should be set, since such a preview would have
    // already committed and we don't want to incorrectly clear that state. If
    // it did, at least make everything functionally correct.
    if (HasEnabledPreviews(initial_state & kPreCommitPreviews)) {
      NOTREACHED();
      previews_data->set_coin_flip_holdback_result(
          CoinFlipHoldbackResult::kNotSet);
      return initial_state;
    }

    UpdatePreviewsUserDataAndRecordCoinFlipResult(
        navigation_handle, previews_data, CoinFlipHoldbackResult::kHoldback);
    return content::PREVIEWS_OFF;
  }

  UpdatePreviewsUserDataAndRecordCoinFlipResult(
      navigation_handle, previews_data, CoinFlipHoldbackResult::kAllowed);
  return initial_state;
}

previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state) {
  // The order is important here.
  if (previews_state & content::OFFLINE_PAGE_ON)
    return previews::PreviewsType::OFFLINE;
  if (previews_state & content::LITE_PAGE_REDIRECT_ON)
    return previews::PreviewsType::LITE_PAGE_REDIRECT;
  if (previews_state & content::SERVER_LITE_PAGE_ON)
    return previews::PreviewsType::LITE_PAGE;
  if (previews_state & content::DEFER_ALL_SCRIPT_ON)
    return previews::PreviewsType::DEFER_ALL_SCRIPT;
  if (previews_state & content::RESOURCE_LOADING_HINTS_ON)
    return previews::PreviewsType::RESOURCE_LOADING_HINTS;
  if (previews_state & content::NOSCRIPT_ON)
    return previews::PreviewsType::NOSCRIPT;

  DCHECK_EQ(content::PREVIEWS_UNSPECIFIED,
            previews_state & ~content::CLIENT_LOFI_AUTO_RELOAD &
                ~content::PREVIEWS_NO_TRANSFORM & ~content::PREVIEWS_OFF);
  return previews::PreviewsType::NONE;
}

}  // namespace previews
