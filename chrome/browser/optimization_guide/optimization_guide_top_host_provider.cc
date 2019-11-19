// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true if hints can't be fetched for |host|.
bool IsHostBlacklisted(const base::DictionaryValue* top_host_blacklist,
                       const std::string& host) {
  if (!top_host_blacklist)
    return false;
  return top_host_blacklist->FindKey(
      optimization_guide::HashHostForDictionary(host));
}

}  // namespace

// static
std::unique_ptr<OptimizationGuideTopHostProvider>
OptimizationGuideTopHostProvider::CreateIfAllowed(
    content::BrowserContext* browser_context) {
  if (IsUserPermittedToFetchHints(
          Profile::FromBrowserContext(browser_context))) {
    return std::make_unique<OptimizationGuideTopHostProvider>(
        browser_context, base::DefaultClock::GetInstance());
  }
  return nullptr;
}

OptimizationGuideTopHostProvider::OptimizationGuideTopHostProvider(
    content::BrowserContext* browser_context,
    base::Clock* time_clock)
    : browser_context_(browser_context),
      time_clock_(time_clock),
      pref_service_(Profile::FromBrowserContext(browser_context_)->GetPrefs()) {
}

OptimizationGuideTopHostProvider::~OptimizationGuideTopHostProvider() {}

void OptimizationGuideTopHostProvider::
    InitializeHintsFetcherTopHostBlacklist() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK_EQ(GetCurrentBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kNotInitialized);
  DCHECK(
      pref_service_
          ->GetDictionary(
              optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist)
          ->empty());

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  std::unique_ptr<base::DictionaryValue> top_host_blacklist =
      std::make_unique<base::DictionaryValue>();

  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();

  std::sort(engagement_details.begin(), engagement_details.end(),
            [](const mojom::SiteEngagementDetails& lhs,
               const mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score > rhs.total_score;
            });

  pref_service_->SetDouble(
      optimization_guide::prefs::kTimeBlacklistLastInitialized,
      time_clock_->Now().ToDeltaSinceWindowsEpoch().InSecondsF());

  // Set the minimum engagement score to -1.0f. This ensures that in the default
  // case (where the blacklist size is enough to accommodate all hosts from the
  // site engagement service), a threshold on the minimum site engagement score
  // does not disqualify |this| from requesting hints for any host.
  pref_service_->SetDouble(
      optimization_guide::prefs::
          kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore,
      -1.0f);

  for (const auto& detail : engagement_details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS())
      continue;
    if (top_host_blacklist->size() >=
        optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize()) {
      // Set the minimum engagement score to the score of the host that
      // could not be added to  |top_host_blacklist|. Add a small epsilon value
      // to the threshold so that any host with score equal to or less than
      // the threshold is not included in the hints fetcher request.
      pref_service_->SetDouble(
          optimization_guide::prefs::
              kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore,
          std::min(detail.total_score + 0.001f,
                   optimization_guide::features::
                       MinTopHostEngagementScoreThreshold()));
      break;
    }
    top_host_blacklist->SetBoolKey(
        optimization_guide::HashHostForDictionary(detail.origin.host()), true);
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "OptimizationGuide.HintsFetcher.TopHostProvider.BlacklistSize."
      "OnInitialize",
      top_host_blacklist->size());

  pref_service_->Set(
      optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist,
      *top_host_blacklist);

  UpdateCurrentBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
          kInitialized);
}

// static
void OptimizationGuideTopHostProvider::MaybeUpdateTopHostBlacklist(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  PrefService* pref_service =
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs();

  if (pref_service->GetInteger(
          optimization_guide::prefs::
              kHintsFetcherDataSaverTopHostBlacklistState) !=
      static_cast<int>(optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kInitialized)) {
    return;
  }

  DictionaryPrefUpdate blacklist_pref(
      pref_service,
      optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist);
  if (!blacklist_pref->FindKey(optimization_guide::HashHostForDictionary(
          navigation_handle->GetURL().host()))) {
    return;
  }
  blacklist_pref->RemovePath(optimization_guide::HashHostForDictionary(
      navigation_handle->GetURL().host()));
  if (blacklist_pref->empty()) {
    blacklist_pref->Clear();
    pref_service->SetInteger(
        optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklistState,
        static_cast<int>(optimization_guide::prefs::
                             HintsFetcherTopHostBlacklistState::kEmpty));
  }
}

optimization_guide::prefs::HintsFetcherTopHostBlacklistState
OptimizationGuideTopHostProvider::GetCurrentBlacklistState() const {
  return static_cast<
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState>(
      pref_service_->GetInteger(
          optimization_guide::prefs::
              kHintsFetcherDataSaverTopHostBlacklistState));
}

void OptimizationGuideTopHostProvider::UpdateCurrentBlacklistState(
    optimization_guide::prefs::HintsFetcherTopHostBlacklistState new_state) {
  optimization_guide::prefs::HintsFetcherTopHostBlacklistState current_state =
      GetCurrentBlacklistState();
  // TODO(mcrouse): Change to DCHECK_NE.
  DCHECK_EQ(
      new_state == optimization_guide::prefs::
                       HintsFetcherTopHostBlacklistState::kInitialized,
      current_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kNotInitialized &&
          new_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kInitialized);

  DCHECK_EQ(
      new_state ==
          optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty,
      current_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kInitialized &&
          new_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kEmpty);

  DCHECK_EQ(new_state == optimization_guide::prefs::
                             HintsFetcherTopHostBlacklistState::kNotInitialized,
            current_state == optimization_guide::prefs::
                                 HintsFetcherTopHostBlacklistState::kEmpty &&
                new_state ==
                    optimization_guide::prefs::
                        HintsFetcherTopHostBlacklistState::kNotInitialized);

  if (current_state == new_state)
    return;

  // TODO(mcrouse): Add histogram to record the blacklist state change.
  pref_service_->SetInteger(
      optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklistState,
      static_cast<int>(new_state));
}

std::vector<std::string> OptimizationGuideTopHostProvider::GetTopHosts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK(pref_service_);

  // It's possible that the blacklist is initialized but
  // kTimeBlacklistLastInitialized pref is not populated. This may happen since
  // the logic to populate kTimeBlacklistLastInitialized pref was added in a
  // later Chrome version. In that case, set kTimeBlacklistLastInitialized to
  // the conservative value of current time.
  if (pref_service_->GetDouble(
          optimization_guide::prefs::kTimeBlacklistLastInitialized) == 0) {
    pref_service_->SetDouble(
        optimization_guide::prefs::kTimeBlacklistLastInitialized,
        time_clock_->Now().ToDeltaSinceWindowsEpoch().InSecondsF());
  }

  if (GetCurrentBlacklistState() ==
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
          kNotInitialized) {
    InitializeHintsFetcherTopHostBlacklist();
    return std::vector<std::string>();
  }

  // Create SiteEngagementService to request site engagement scores.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  const base::DictionaryValue* top_host_blacklist = nullptr;
  if (GetCurrentBlacklistState() !=
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty) {
    top_host_blacklist = pref_service_->GetDictionary(
        optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist);
    UMA_HISTOGRAM_COUNTS_1000(
        "OptimizationGuide.HintsFetcher.TopHostProvider.BlacklistSize."
        "OnRequest",
        top_host_blacklist->size());
    // This check likely should not be needed as the process of removing hosts
    // from the blacklist should check and update the pref state.
    if (top_host_blacklist->size() == 0) {
      UpdateCurrentBlacklistState(
          optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty);
      top_host_blacklist = nullptr;
    }
  }

  std::vector<std::string> top_hosts;

  // Create a vector of the top hosts by engagement score up to |max_sites|
  // size. Currently utilizes just the first |max_sites| entries. Only HTTPS
  // schemed hosts are included. Hosts are filtered by the blacklist that is
  // populated when DataSaver is first enabled.
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();

  std::sort(engagement_details.begin(), engagement_details.end(),
            [](const mojom::SiteEngagementDetails& lhs,
               const mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score > rhs.total_score;
            });

  base::Time blacklist_initialized_time =
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSecondsD(pref_service_->GetDouble(
              optimization_guide::prefs::kTimeBlacklistLastInitialized)));

  base::TimeDelta duration_since_blacklist_initialized =
      (time_clock_->Now() - blacklist_initialized_time);

  for (const auto& detail : engagement_details) {
    if (!detail.origin.SchemeIs(url::kHttpsScheme))
      continue;
    // Once the engagement score is less than the initial engagement score for a
    // newly navigated host, return the current set of top hosts. This threshold
    // prevents hosts that have not been engaged recently from having hints
    // requested for them. The engagement_details are sorted above in descending
    // order by engagement score.
    // This filtering is applied only if the the blacklist was initialized
    // recently. If the blacklist was initialized too far back in time, hosts
    // that could not make it to blacklist should have either been navigated to
    // or would have fallen off the blacklist.
    if (duration_since_blacklist_initialized <=
            optimization_guide::features::
                DurationApplyLowEngagementScoreThreshold() &&
        detail.total_score <
            pref_service_->GetDouble(
                optimization_guide::prefs::
                    kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore)) {
      return top_hosts;
    }
    if (!IsHostBlacklisted(top_host_blacklist, detail.origin.host())) {
      top_hosts.push_back(detail.origin.host());
    }
  }

  return top_hosts;
}
