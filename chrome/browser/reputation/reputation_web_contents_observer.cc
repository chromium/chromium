// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/reputation_web_contents_observer.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_visibility_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace {

// Whether to show tips on server-side-flagged sites included in the component.
const base::FeatureParam<bool> kEnableSuspiciousSiteChecks{
    &security_state::features::kSafetyTipUI, "suspicioussites", true};

void RecordHeuristicsUKMData(ReputationCheckResult result,
                             ukm::SourceId navigation_source_id,
                             SafetyTipInteraction action) {
  // If we didn't trigger any heuristics at all, we don't want to record UKM
  // data.
  if (!result.triggered_heuristics.triggered_any()) {
    return;
  }

  ukm::builders::Security_SafetyTip(navigation_source_id)
      .SetSafetyTipStatus(static_cast<int64_t>(result.safety_tip_status))
      .SetSafetyTipInteraction(static_cast<int64_t>(action))
      .SetTriggeredKeywordsHeuristics(
          result.triggered_heuristics.keywords_heuristic_triggered)
      .SetTriggeredLookalikeHeuristics(
          result.triggered_heuristics.lookalike_heuristic_triggered)
      .SetTriggeredServerSideBlocklist(
          result.triggered_heuristics.blocklist_heuristic_triggered)
      .SetUserPreviouslyIgnored(
          result.safety_tip_status ==
              security_state::SafetyTipStatus::kBadReputationIgnored ||
          result.safety_tip_status ==
              security_state::SafetyTipStatus::kLookalikeIgnored)
      .Record(ukm::UkmRecorder::Get());
}

void OnSafetyTipClosed(ReputationCheckResult result,
                       base::Time start_time,
                       ukm::SourceId navigation_source_id,
                       Profile* profile,
                       const GURL& url,
                       security_state::SafetyTipStatus status,
                       base::OnceClosure safety_tip_close_callback_for_testing,
                       SafetyTipInteraction action) {
  std::string action_suffix;
  bool warning_dismissed = false;
  switch (action) {
    case SafetyTipInteraction::kNoAction:
      action_suffix = "NoAction";
      break;
    case SafetyTipInteraction::kLeaveSite:
      action_suffix = "LeaveSite";
      break;
    case SafetyTipInteraction::kDismiss:
      NOTREACHED();
      // Do nothing because the dismissal action passed to this method should
      // be the more specific version (esc, close, or ignore).
      break;
    case SafetyTipInteraction::kDismissWithEsc:
      action_suffix = "DismissWithEsc";
      warning_dismissed = true;
      break;
    case SafetyTipInteraction::kDismissWithClose:
      action_suffix = "DismissWithClose";
      warning_dismissed = true;
      break;
    case SafetyTipInteraction::kDismissWithIgnore:
      action_suffix = "DismissWithIgnore";
      warning_dismissed = true;
      break;
    case SafetyTipInteraction::kLearnMore:
      action_suffix = "LearnMore";
      break;
    case SafetyTipInteraction::kNotShown:
      NOTREACHED();
      // Do nothing because the OnSafetyTipClosed should never be called if the
      // safety tip is not shown.
      break;
    case SafetyTipInteraction::kCloseTab:
      action_suffix = "CloseTab";
      break;
    case SafetyTipInteraction::kSwitchTab:
      action_suffix = "SwitchTab";
      break;
    case SafetyTipInteraction::kStartNewNavigation:
      action_suffix = "StartNewNavigation";
      break;
  }
  if (warning_dismissed) {
    ReputationService::Get(profile)->SetUserIgnore(url);

    // Record that the user dismissed the safety tip. kDismiss is recorded in
    // all dismiss-like cases, which makes it easier to track overall dismissals
    // without having to re-constitute from each bucket on how the user
    // dismissed the safety tip. We additionally record a more specific action
    // below (e.g. kDismissWithEsc).
    base::UmaHistogramEnumeration(
        security_state::GetSafetyTipHistogramName(
            "Security.SafetyTips.Interaction", status),
        SafetyTipInteraction::kDismiss);
    base::UmaHistogramCustomTimes(
        security_state::GetSafetyTipHistogramName(
            std::string("Security.SafetyTips.OpenTime.Dismiss"),
            result.safety_tip_status),
        base::Time::Now() - start_time, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromHours(1), 100);
  }
  base::UmaHistogramEnumeration(security_state::GetSafetyTipHistogramName(
                                    "Security.SafetyTips.Interaction", status),
                                action);
  base::UmaHistogramCustomTimes(
      security_state::GetSafetyTipHistogramName(
          std::string("Security.SafetyTips.OpenTime.") + action_suffix,
          result.safety_tip_status),
      base::Time::Now() - start_time, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromHours(1), 100);

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
    const base::Optional<url::Origin>& committed_initiator_origin,
    const GURL& committed_url,
    const GURL& current_url,
    security_state::SafetyTipStatus status) {
  std::string suffix;
  if (committed_url != current_url) {
    // So long as we only record this metric following DidFinishNavigation, not
    // OnVisibilityChanged, this should rarely happen. It would mean that a new
    // navigation committed in this web contents before the reputation check
    // completed. This is possible only when engaged_sites is out of date
    // (forcing an async update). In that scenario, there may be a race
    // condition between the async reputation check completing and the next call
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
  } else if (GetETLDPlusOne(committed_initiator_origin.value().host()) ==
             GetETLDPlusOne(current_url.host())) {
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

// Returns whether a safety tip should be shown, according to finch.
bool IsSafetyTipEnabled(security_state::SafetyTipStatus status) {
  if (!security_state::IsSafetyTipUIFeatureEnabled()) {
    return false;
  }

  if (status != security_state::SafetyTipStatus::kBadReputation) {
    return true;
  }

  // Safety Tips can be enabled with a few different features that have slightly
  // different behavior. "Suspicious site" Safety Tips are enabled for the main
  // Safety Tip feature, |kSafetyTipUI|, by a parameter, and they are always
  // enabled for the delayed warnings Safety Tip feature (which uses "Suspicious
  // site" Safety Tips on phishing pages blocking by Safe Browsing.)
  if (base::FeatureList::IsEnabled(security_state::features::kSafetyTipUI)) {
    return kEnableSuspiciousSiteChecks.Get();
  }

  return base::FeatureList::IsEnabled(
      security_state::features::kSafetyTipUIOnDelayedWarning);
}

}  // namespace

ReputationWebContentsObserver::~ReputationWebContentsObserver() = default;

void ReputationWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    MaybeCallReputationCheckCallback(false);
    return;
  }

  // Same doc navigations keep the same status as their predecessor. Update last
  // navigation entry so that GetSafetyTipInfoForVisibleNavigation() works.
  if (navigation_handle->IsSameDocument()) {
    last_safety_tip_navigation_entry_id_ =
        web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();
    MaybeCallReputationCheckCallback(false);
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

void ReputationWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  MaybeShowSafetyTip(ukm::GetSourceIdForWebContentsDocument(web_contents()),
                     /*called_from_visibility_check=*/true,
                     /*record_ukm_if_tip_not_shown=*/false);
}

security_state::SafetyTipInfo
ReputationWebContentsObserver::GetSafetyTipInfoForVisibleNavigation() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return {security_state::SafetyTipStatus::kUnknown, GURL()};
  return last_safety_tip_navigation_entry_id_ == entry->GetUniqueID()
             ? last_navigation_safety_tip_info_
             : security_state::SafetyTipInfo(
                   {security_state::SafetyTipStatus::kUnknown, GURL()});
}

void ReputationWebContentsObserver::RegisterReputationCheckCallbackForTesting(
    base::OnceClosure callback) {
  reputation_check_callback_for_testing_ = std::move(callback);
}

void ReputationWebContentsObserver::RegisterSafetyTipCloseCallbackForTesting(
    base::OnceClosure callback) {
  safety_tip_close_callback_for_testing_ = std::move(callback);
}

ReputationWebContentsObserver::ReputationWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      reputation_check_pending_for_testing_(true),
      weak_factory_(this) {
  last_navigation_safety_tip_info_ = {security_state::SafetyTipStatus::kUnknown,
                                      GURL()};
}

void ReputationWebContentsObserver::MaybeShowSafetyTip(
    ukm::SourceId navigation_source_id,
    bool called_from_visibility_check,
    bool record_ukm_if_tip_not_shown) {
  if (web_contents()->GetMainFrame()->GetVisibilityState() !=
      content::PageVisibilityState::kVisible) {
    MaybeCallReputationCheckCallback(false);
    return;
  }

  // Filter out loads with no navigations, error pages and interstitials.
  auto* last_entry = web_contents()->GetController().GetLastCommittedEntry();
  if (!last_entry || last_entry->GetPageType() != content::PAGE_TYPE_NORMAL) {
    MaybeCallReputationCheckCallback(false);
    return;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    MaybeCallReputationCheckCallback(false);
    return;
  }

  ReputationService* service = ReputationService::Get(profile_);
  service->GetReputationStatus(
      url, web_contents(),
      base::BindOnce(
          &ReputationWebContentsObserver::HandleReputationCheckResult,
          weak_factory_.GetWeakPtr(), navigation_source_id,
          called_from_visibility_check, record_ukm_if_tip_not_shown));
}

void ReputationWebContentsObserver::HandleReputationCheckResult(
    ukm::SourceId navigation_source_id,
    bool called_from_visibility_check,
    bool record_ukm_if_tip_not_shown,
    ReputationCheckResult result) {
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

  // A navigation entry should always exist because reputation checks are only
  // triggered when a committed navigation finishes.
  last_safety_tip_navigation_entry_id_ =
      web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();
  // Since we downgrade indicator when a safety tip is triggered, update the
  // visible security state if we have a non-kNone status. This has to happen
  // after last_safety_tip_navigation_entry_id_ is updated.
  if (result.safety_tip_status != security_state::SafetyTipStatus::kNone) {
    web_contents()->DidChangeVisibleSecurityState();
  }

  if (result.safety_tip_status == security_state::SafetyTipStatus::kNone ||
      result.safety_tip_status ==
          security_state::SafetyTipStatus::kBadKeyword) {
    FinalizeReputationCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                           navigation_source_id);
    return;
  }

  if (result.safety_tip_status ==
          security_state::SafetyTipStatus::kLookalikeIgnored ||
      result.safety_tip_status ==
          security_state::SafetyTipStatus::kBadReputationIgnored) {
    UMA_HISTOGRAM_ENUMERATION("Security.SafetyTips.SafetyTipIgnoredPageLoad",
                              result.safety_tip_status);
    FinalizeReputationCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                           navigation_source_id);
    return;
  }

  // Log a console message if it's the first time we're going to open the Safety
  // Tip. (Otherwise, we'd print the message each time the tab became visible.)
  if (!called_from_visibility_check) {
    web_contents()->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "Chrome has determined that %s could be fake or fraudulent.\n\n"
            "If you believe this is shown in error please visit "
            "https://g.co/chrome/lookalike-warnings",
            result.url.host().c_str()));
  }

  if (!IsSafetyTipEnabled(result.safety_tip_status)) {
    // When the feature isn't enabled, we 'ignore' the UI after the first visit
    // to make it easier to disambiguate the control groups' first visit from
    // subsequent navigations to the flagged page in metrics. Since the user
    // never sees the UI, this is a no-op from their perspective.
    if (result.safety_tip_status ==
            security_state::SafetyTipStatus::kLookalike ||
        result.safety_tip_status ==
            security_state::SafetyTipStatus::kBadReputation) {
      ReputationService::Get(profile_)->OnUIDisabledFirstVisit(result.url);
    }

    RecordPostFlagCheckHistogram(result.safety_tip_status);
    FinalizeReputationCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                           navigation_source_id);
    return;
  }

  if (!base::FeatureList::IsEnabled(
          lookalikes::features::kLookalikeDigitalAssetLinks) ||
      !result.suggested_url.is_valid()) {
    RecordPostFlagCheckHistogram(result.safety_tip_status);
    ShowSafetyTipDialog(
        web_contents(), result.safety_tip_status, result.suggested_url,
        base::BindOnce(OnSafetyTipClosed, result, base::Time::Now(),
                       navigation_source_id, profile_, result.url,
                       result.safety_tip_status,
                       std::move(safety_tip_close_callback_for_testing_)));
    MaybeCallReputationCheckCallback(true);
    return;
  }

  const url::Origin lookalike_origin = url::Origin::Create(result.url);
  const url::Origin target_origin = url::Origin::Create(result.suggested_url);

  DigitalAssetLinkCrossValidator::ResultCallback callback = base::BindOnce(
      &ReputationWebContentsObserver::OnDigitalAssetLinkValidationResult,
      weak_factory_.GetWeakPtr(), result, navigation_source_id);
  digital_asset_link_validator_ =
      std::make_unique<DigitalAssetLinkCrossValidator>(
          profile_, lookalike_origin, target_origin,
          LookalikeUrlService::kManifestFetchDelay.Get(),
          LookalikeUrlService::Get(profile_)->clock(), std::move(callback));
  digital_asset_link_validator_->Start();
}

void ReputationWebContentsObserver::OnDigitalAssetLinkValidationResult(
    ReputationCheckResult result,
    ukm::SourceId navigation_source_id,
    bool validation_succeeded) {
  if (validation_succeeded) {
    // Don't show a safety tip dialog.
    base::UmaHistogramEnumeration(
        "Security.SafetyTips.ReputationCheckComplete.DidFinishNavigation",
        security_state::SafetyTipStatus::kDigitalAssetLinkMatch);
    RecordPostFlagCheckHistogram(
        security_state::SafetyTipStatus::kDigitalAssetLinkMatch);
    MaybeCallReputationCheckCallback(/*heuristics_checked=*/true);
    return;
  }

  RecordPostFlagCheckHistogram(result.safety_tip_status);

  ShowSafetyTipDialog(
      web_contents(), result.safety_tip_status, result.suggested_url,
      base::BindOnce(OnSafetyTipClosed, result, base::Time::Now(),
                     navigation_source_id, profile_, result.url,
                     result.safety_tip_status,
                     std::move(safety_tip_close_callback_for_testing_)));
  MaybeCallReputationCheckCallback(/*heuristics_checked=*/true);
}

void ReputationWebContentsObserver::MaybeCallReputationCheckCallback(
    bool heuristics_checked) {
  if (heuristics_checked)
    reputation_check_pending_for_testing_ = false;
  if (reputation_check_callback_for_testing_.is_null())
    return;
  std::move(reputation_check_callback_for_testing_).Run();
}

void ReputationWebContentsObserver::FinalizeReputationCheckWhenTipNotShown(
    bool record_ukm,
    ReputationCheckResult result,
    ukm::SourceId navigation_source_id) {
  if (record_ukm) {
    RecordHeuristicsUKMData(result, navigation_source_id,
                            SafetyTipInteraction::kNotShown);
  }
  MaybeCallReputationCheckCallback(true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReputationWebContentsObserver)
