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

  net::IPAddress ip_addr;
  if (url.HostIsIPAddress() && ip_addr.AssignFromIPLiteral(url.host()) &&
      !ip_addr.IsPubliclyRoutable()) {
    return true;
  }
  return false;
}

bool HasEnabledPreviews(blink::PreviewsState previews_state) {
  return previews_state != blink::PreviewsTypes::PREVIEWS_UNSPECIFIED &&
         !(previews_state & blink::PreviewsTypes::PREVIEWS_OFF) &&
         !(previews_state & blink::PreviewsTypes::PREVIEWS_NO_TRANSFORM);
}

blink::PreviewsState DetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    bool previews_triggering_logic_already_ran,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle) {
  blink::PreviewsState previews_state =
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;

  const GURL& url = navigation_handle->GetURL();
  bool is_reload =
      navigation_handle->GetReloadType() != content::ReloadType::NONE;

  if (!previews::params::ArePreviewsAllowed()) {
    return previews_state;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return previews_state;
  }

  if (!is_data_saver_user)
    return previews_state;

  if (previews_triggering_logic_already_ran) {
    // Record that the navigation was redirected.
    previews_data->set_is_redirect(true);
  }

  // Check commit-time preview types first.
  if (previews_decider->ShouldAllowPreviewAtNavigationStart(
          previews_data, navigation_handle, is_reload,
          previews::PreviewsType::DEFER_ALL_SCRIPT)) {
    previews_state |= blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
  }

  return previews_state;
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

blink::PreviewsState DetermineCommittedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    blink::PreviewsState previews_state,
    const previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle) {
  if (previews_data && previews_data->cache_control_no_transform_directive()) {
    if (HasEnabledPreviews(previews_state)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.CacheControlNoTransform.BlockedPreview",
          GetMainFramePreviewsType(previews_state),
          previews::PreviewsType::LAST);
    }
    return blink::PreviewsTypes::PREVIEWS_OFF;
  }

  // Check if the URL is eligible for defer all script preview. A URL
  // may not be eligible for the preview if it's likely to cause a
  // client redirect loop.
  if ((previews::params::DetectDeferRedirectLoopsUsingCache()) &&
      (previews_state & blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON)) {
    content::WebContents* web_contents =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;
    if (web_contents) {
      auto* previews_service = PreviewsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

      if (previews_service &&
          !previews_service->IsUrlEligibleForDeferAllScriptPreview(url)) {
        previews_state &= ~blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
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

  if (previews_state & blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON) {
    content::WebContents* web_contents =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;
    if (web_contents) {
      auto* previews_service = PreviewsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

      if (previews_service &&
          previews_service->MatchesDeferAllScriptDenyListRegexp(url)) {
        previews_state &= ~blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
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

  if (previews_state & blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON) {
    // DeferAllScript was allowed for the original URL but only continue with it
    // if the committed URL has HTTPS scheme and is allowed by decider.
    if (previews_decider && previews_decider->ShouldCommitPreview(
                                previews_data, navigation_handle,
                                previews::PreviewsType::DEFER_ALL_SCRIPT)) {
      return blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
    }
    // Remove DEFER_ALL_SCRIPT_ON from |previews_state| since we decided not to
    // commit to it.
    previews_state =
        previews_state & ~blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
  }

  if (!previews_state) {
    return blink::PreviewsTypes::PREVIEWS_OFF;
  }

  DCHECK(previews_state == blink::PreviewsTypes::PREVIEWS_OFF ||
         previews_state == blink::PreviewsTypes::PREVIEWS_UNSPECIFIED);
  return blink::PreviewsTypes::PREVIEWS_OFF;
}

blink::PreviewsState MaybeCoinFlipHoldbackAfterCommit(
    blink::PreviewsState initial_state,
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
    UpdatePreviewsUserDataAndRecordCoinFlipResult(
        navigation_handle, previews_data, CoinFlipHoldbackResult::kHoldback);
    return blink::PreviewsTypes::PREVIEWS_OFF;
  }

  UpdatePreviewsUserDataAndRecordCoinFlipResult(
      navigation_handle, previews_data, CoinFlipHoldbackResult::kAllowed);
  return initial_state;
}

previews::PreviewsType GetMainFramePreviewsType(
    blink::PreviewsState previews_state) {
  // The order is important here.
  if (previews_state & blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON)
    return previews::PreviewsType::DEFER_ALL_SCRIPT;

  DCHECK_EQ(blink::PreviewsTypes::PREVIEWS_UNSPECIFIED,
            previews_state & ~blink::PreviewsTypes::PREVIEWS_NO_TRANSFORM &
                ~blink::PreviewsTypes::PREVIEWS_OFF);
  return previews::PreviewsType::NONE;
}

}  // namespace previews
