// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_service_factory.h"
#include "chrome/browser/lookalikes/safety_tip_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_visibility_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace {

void RecordHeuristicsUKMData(SafetyTipCheckResult result,
                             ukm::SourceId navigation_source_id,
                             SafetyTipInteraction action) {
  DCHECK(
      result.safety_tip_status == security_state::SafetyTipStatus::kNone ||
      result.safety_tip_status == security_state::SafetyTipStatus::kUnknown ||
      result.safety_tip_status == security_state::SafetyTipStatus::kLookalike ||
      result.safety_tip_status ==
          security_state::SafetyTipStatus::kLookalikeIgnored);

  // If we didn't trigger any lookalike heuristics at all, we don't want to
  // record UKM data.
  if (!result.lookalike_heuristic_triggered) {
    return;
  }

  ukm::builders::Security_SafetyTip(navigation_source_id)
      .SetSafetyTipStatus(static_cast<int64_t>(result.safety_tip_status))
      .SetSafetyTipInteraction(static_cast<int64_t>(action))
      .SetTriggeredLookalikeHeuristics(true)
      .SetTriggeredServerSideBlocklist(false) /* Deprecated */
      .SetTriggeredKeywordsHeuristics(false)  /* Deprecated */
      .SetUserPreviouslyIgnored(
          result.safety_tip_status ==
          security_state::SafetyTipStatus::kLookalikeIgnored)
      .Record(ukm::UkmRecorder::Get());
}

void OnSafetyTipClosed(SafetyTipCheckResult result,
                       ukm::SourceId navigation_source_id,
                       Profile* profile,
                       const GURL& url,
                       security_state::SafetyTipStatus status,
                       base::OnceClosure safety_tip_close_callback_for_testing,
                       SafetyTipInteraction action) {
  if (action == SafetyTipInteraction::kDismissWithEsc ||
      action == SafetyTipInteraction::kDismissWithClose ||
      action == SafetyTipInteraction::kDismissWithIgnore) {
    LookalikeUrlServiceFactory::GetForProfile(profile)->SetUserIgnore(url);

    // Record that the user dismissed the safety tip. kDismiss is recorded in
    // all dismiss-like cases, which makes it easier to track overall dismissals
    // without having to re-constitute from each bucket on how the user
    // dismissed the safety tip. We  also record a more specific action
    // below (e.g. kDismissWithEsc).
    base::UmaHistogramEnumeration(
        security_state::GetSafetyTipHistogramName(
            "Security.SafetyTips.Interaction", status),
        SafetyTipInteraction::kDismiss);
  }
  base::UmaHistogramEnumeration(security_state::GetSafetyTipHistogramName(
                                    "Security.SafetyTips.Interaction", status),
                                action);
  RecordHeuristicsUKMData(result, navigation_source_id, action);

  if (!safety_tip_close_callback_for_testing.is_null()) {
    std::move(safety_tip_close_callback_for_testing).Run();
  }
}

// Safety Tips does not use starts_active (since flagged sites are so rare to
// begin with), so this function records the same metric as "SafetyTipShown",
// but does so after the flag check, which may impact flag recording.
void RecordPostFlagCheckHistogram(security_state::SafetyTipStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Security.SafetyTips.SafetyTipShown_AfterFlag",
                            status);
}

// Records a histogram that embeds the safety tip status along with whether the
// navigation was initiated cross- or same-origin.
void RecordSafetyTipStatusWithInitiatorOriginInfo(
    const std::optional<url::Origin>& committed_initiator_origin,
    const GURL& committed_url,
    const GURL& current_url,
    security_state::SafetyTipStatus status) {
  std::string suffix;
  if (committed_url != current_url) {
    // So long as we only record this metric following DidFinishNavigation, not
    // OnVisibilityChanged, this should rarely happen. It would mean that a new
    // navigation committed in this web contents before the safety tip check
    // completed. This is possible only when engaged_sites is out of date
    // (forcing an async update). In that scenario, there may be a race
    // condition between the async safety tip check completing and the next call
    // to DidFinishNavigation.
    suffix = "UnexpectedUrl";
  } else if (!committed_initiator_origin.has_value()) {
    // The initiator origin has no value in cases like omnibox-initiated, or
    // outside-of-Chrome-initiated, navigations.
    suffix = "Unknown";
  } else if (committed_initiator_origin.value().CanBeDerivedFrom(current_url)) {
    // This is assumed to mean that the user has clicked on a same-origin link
    // on a lookalike page, resulting in another lookalike navigation.
    suffix = "SameOrigin";
  } else if (lookalikes::GetETLDPlusOne(
                 committed_initiator_origin.value().host()) ==
             lookalikes::GetETLDPlusOne(current_url.host())) {
    // The user has clicked on a link on a page, and it's bumped to another
    // page on the same eTLD+1. If that happens and this is a non-none and
    // non-ignored status, that implies that the first eTLD+1 load didn't
    // trigger the warning, this subsequent page load did, implying that it was
    // triggered by a different subdomain.
    suffix = "SameRegDomain";
  } else {
    // This is assumed to mean that the user has clicked on a link from a
    // non-lookalike page, newly triggering the safety tip.
    suffix = "CrossOrigin";
  }

  base::UmaHistogramEnumeration(
      "Security.SafetyTips.StatusWithInitiator." + suffix, status);
}

}  // namespace

SafetyTipWebContentsObserver::~SafetyTipWebContentsObserver() = default;

void SafetyTipWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    MaybeCallSafetyTipCheckCallback(false);
    return;
  }

  // Same doc navigations keep the same status as their predecessor. Update last
  // navigation entry so that GetSafetyTipInfoForVisibleNavigation() works.
  if (navigation_handle->IsSameDocument()) {
    last_safety_tip_navigation_entry_id_ =
        web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();
    MaybeCallSafetyTipCheckCallback(false);
    return;
  }

  last_navigation_safety_tip_info_ = {security_state::SafetyTipStatus::kUnknown,
                                      GURL()};
  last_safety_tip_navigation_entry_id_ = 0;
  last_committed_initiator_origin_ = navigation_handle->GetInitiatorOrigin();
  last_committed_url_ = navigation_handle->GetURL();

  MaybeShowSafetyTip(
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID),
      /*called_from_visibility_check=*/false,
      /*record_ukm_if_tip_not_shown=*/true);
}

void SafetyTipWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  MaybeShowSafetyTip(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
      /*called_from_visibility_check=*/true,
      /*record_ukm_if_tip_not_shown=*/false);
}

security_state::SafetyTipInfo
SafetyTipWebContentsObserver::GetSafetyTipInfoForVisibleNavigation() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry) {
    return {security_state::SafetyTipStatus::kUnknown, GURL()};
  }
  return last_safety_tip_navigation_entry_id_ == entry->GetUniqueID()
             ? last_navigation_safety_tip_info_
             : security_state::SafetyTipInfo(
                   {security_state::SafetyTipStatus::kUnknown, GURL()});
}

void SafetyTipWebContentsObserver::RegisterSafetyTipCheckCallbackForTesting(
    base::OnceClosure callback) {
  safety_tip_check_callback_for_testing_ = std::move(callback);
}

void SafetyTipWebContentsObserver::RegisterSafetyTipCloseCallbackForTesting(
    base::OnceClosure callback) {
  safety_tip_close_callback_for_testing_ = std::move(callback);
}

SafetyTipWebContentsObserver::SafetyTipWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<SafetyTipWebContentsObserver>(*web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      weak_factory_(this) {
  last_navigation_safety_tip_info_ = {security_state::SafetyTipStatus::kUnknown,
                                      GURL()};
}

void SafetyTipWebContentsObserver::MaybeShowSafetyTip(
    ukm::SourceId navigation_source_id,
    bool called_from_visibility_check,
    bool record_ukm_if_tip_not_shown) {
  if (web_contents()->GetPrimaryMainFrame()->GetVisibilityState() !=
      content::PageVisibilityState::kVisible) {
    MaybeCallSafetyTipCheckCallback(false);
    return;
  }

  // Filter out loads with no navigations, error pages and interstitials.
  auto* last_entry = web_contents()->GetController().GetLastCommittedEntry();
  if (!last_entry || last_entry->GetPageType() != content::PAGE_TYPE_NORMAL) {
    MaybeCallSafetyTipCheckCallback(false);
    return;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    MaybeCallSafetyTipCheckCallback(false);
    return;
  }

  LookalikeUrlServiceFactory::GetForProfile(profile_)->CheckSafetyTipStatus(
      url, web_contents(),
      base::BindOnce(&SafetyTipWebContentsObserver::HandleSafetyTipCheckResult,
                     weak_factory_.GetWeakPtr(), navigation_source_id,
                     called_from_visibility_check,
                     record_ukm_if_tip_not_shown));
}

void SafetyTipWebContentsObserver::HandleSafetyTipCheckResult(
    ukm::SourceId navigation_source_id,
    bool called_from_visibility_check,
    bool record_ukm_if_tip_not_shown,
    SafetyTipCheckResult result) {
  UMA_HISTOGRAM_ENUMERATION("Security.SafetyTips.SafetyTipShown",
                            result.safety_tip_status);
  base::UmaHistogramEnumeration(
      called_from_visibility_check
          ? "Security.SafetyTips.ReputationCheckComplete.VisibilityChanged"
          : "Security.SafetyTips.ReputationCheckComplete.DidFinishNavigation",
      result.safety_tip_status);
  if (!called_from_visibility_check) {
    RecordSafetyTipStatusWithInitiatorOriginInfo(
        last_committed_initiator_origin_, last_committed_url_, result.url,
        result.safety_tip_status);
  }

  // Set this field independent of whether the feature to show the UI is
  // enabled/disabled. Metrics code uses this field and we want to record
  // metrics regardless of the feature being enabled/disabled.
  last_navigation_safety_tip_info_ = {result.safety_tip_status,
                                      result.suggested_url};

  // A navigation entry should always exist because safety tip checks are only
  // triggered when a committed navigation finishes.
  last_safety_tip_navigation_entry_id_ =
      web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();
  // Since we downgrade indicator when a safety tip is triggered, update the
  // visible security state if we have a non-kNone status. This has to happen
  // after last_safety_tip_navigation_entry_id_ is updated.
  if (result.safety_tip_status != security_state::SafetyTipStatus::kNone) {
    web_contents()->DidChangeVisibleSecurityState();
  }

  if (result.safety_tip_status == security_state::SafetyTipStatus::kNone) {
    FinalizeSafetyTipCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                          navigation_source_id);
    return;
  }

  if (result.safety_tip_status ==
      security_state::SafetyTipStatus::kLookalikeIgnored) {
    UMA_HISTOGRAM_ENUMERATION("Security.SafetyTips.SafetyTipIgnoredPageLoad",
                              result.safety_tip_status);
    FinalizeSafetyTipCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                          navigation_source_id);
    return;
  }

  // Log a console message if it's the first time we're going to open the Safety
  // Tip. (Otherwise, we'd print the message each time the tab became visible.)
  if (!called_from_visibility_check) {
    web_contents()->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        lookalikes::GetConsoleMessage(result.url,
                                      /*is_new_heuristic=*/false));
  }

  RecordPostFlagCheckHistogram(result.safety_tip_status);

  base::OnceCallback<void(SafetyTipInteraction)> close_callback =
      base::BindOnce(OnSafetyTipClosed, result, navigation_source_id, profile_,
                     result.url, result.safety_tip_status,
                     std::move(safety_tip_close_callback_for_testing_));

#if BUILDFLAG(IS_ANDROID)
  delegate_.DisplaySafetyTipPrompt(result.safety_tip_status,
                                   result.suggested_url, web_contents(),
                                   std::move(close_callback));
#else

  ShowSafetyTipDialog(web_contents(), result.safety_tip_status,
                      result.suggested_url, std::move(close_callback));
#endif
  MaybeCallSafetyTipCheckCallback(true);
}

void SafetyTipWebContentsObserver::MaybeCallSafetyTipCheckCallback(
    bool heuristics_checked) {
  if (heuristics_checked) {
    safety_tip_check_pending_for_testing_ = false;
  }
  if (safety_tip_check_callback_for_testing_.is_null()) {
    return;
  }
  std::move(safety_tip_check_callback_for_testing_).Run();
}

void SafetyTipWebContentsObserver::FinalizeSafetyTipCheckWhenTipNotShown(
    bool record_ukm,
    SafetyTipCheckResult result,
    ukm::SourceId navigation_source_id) {
  if (record_ukm) {
    RecordHeuristicsUKMData(result, navigation_source_id,
                            SafetyTipInteraction::kNotShown);
  }
  MaybeCallSafetyTipCheckCallback(true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafetyTipWebContentsObserver);
