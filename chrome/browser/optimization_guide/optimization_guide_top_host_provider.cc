// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true if hints can't be fetched for |host|.
bool IsHostBlocklisted(const base::DictionaryValue* top_host_blocklist,
                       const std::string& host) {
  if (!top_host_blocklist)
    return false;
  return top_host_blocklist->FindKey(
      optimization_guide::HashHostForDictionary(host));
}

// Return the current state of the HintsFetcherTopHostBlocklist held in the
// |kHintsFetcherTopHostBlocklistState| pref.
optimization_guide::prefs::HintsFetcherTopHostBlocklistState
GetCurrentBlocklistState(PrefService* pref_service) {
  return static_cast<
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState>(
      pref_service->GetInteger(
          optimization_guide::prefs::kHintsFetcherTopHostBlocklistState));
}

// Updates the state of the HintsFetcherTopHostBlocklist to |new_state|.
void UpdateCurrentBlocklistState(
    PrefService* pref_service,
    optimization_guide::prefs::HintsFetcherTopHostBlocklistState new_state) {
  optimization_guide::prefs::HintsFetcherTopHostBlocklistState current_state =
      GetCurrentBlocklistState(pref_service);
  DCHECK_EQ(
      new_state == optimization_guide::prefs::
                       HintsFetcherTopHostBlocklistState::kInitialized,
      current_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlocklistState::kNotInitialized &&
          new_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlocklistState::kInitialized);

  DCHECK_EQ(
      new_state ==
          optimization_guide::prefs::HintsFetcherTopHostBlocklistState::kEmpty,
      current_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlocklistState::kInitialized &&
          new_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlocklistState::kEmpty);

  // Any state can go to not initialized, so no need to check here.

  if (current_state == new_state)
    return;

  // TODO(mcrouse): Add histogram to record the blocklist state change.
  pref_service->SetInteger(
      optimization_guide::prefs::kHintsFetcherTopHostBlocklistState,
      static_cast<int>(new_state));
}

// Resets the state of the HintsFetcherTopHostBlocklist held in the
// kHintsFetcherTopHostBlocklistState| pref to
// |optimization_guide::prefs::HintsFetcherTopHostBlocklistState::kNotInitialized|.
void ResetTopHostBlocklistState(PrefService* pref_service) {
  if (GetCurrentBlocklistState(pref_service) ==
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
          kNotInitialized) {
    return;
  }
  DictionaryPrefUpdate blocklist_pref(
      pref_service, optimization_guide::prefs::kHintsFetcherTopHostBlocklist);
  blocklist_pref->Clear();
  UpdateCurrentBlocklistState(
      pref_service, optimization_guide::prefs::
                        HintsFetcherTopHostBlocklistState::kNotInitialized);
}

}  // namespace

// static
std::unique_ptr<OptimizationGuideTopHostProvider>
OptimizationGuideTopHostProvider::CreateIfAllowed(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), profile->GetPrefs())) {
    return base::WrapUnique(new OptimizationGuideTopHostProvider(
        browser_context, base::DefaultClock::GetInstance()));
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
    InitializeHintsFetcherTopHostBlocklist() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK_EQ(GetCurrentBlocklistState(pref_service_),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kNotInitialized);
  DCHECK(pref_service_
             ->GetDictionary(
                 optimization_guide::prefs::kHintsFetcherTopHostBlocklist)
             ->empty());

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  auto* engagement_service =
      site_engagement::SiteEngagementService::Get(profile);

  std::unique_ptr<base::DictionaryValue> top_host_blocklist =
      std::make_unique<base::DictionaryValue>();

  std::vector<site_engagement::mojom::SiteEngagementDetails>
      engagement_details = engagement_service->GetAllDetails();

  std::sort(engagement_details.begin(), engagement_details.end(),
            [](const site_engagement::mojom::SiteEngagementDetails& lhs,
               const site_engagement::mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score > rhs.total_score;
            });

  pref_service_->SetDouble(
      optimization_guide::prefs::
          kTimeHintsFetcherTopHostBlocklistLastInitialized,
      time_clock_->Now().ToDeltaSinceWindowsEpoch().InSecondsF());

  // Set the minimum engagement score to -1.0f. This ensures that in the default
  // case (where the blocklist size is enough to accommodate all hosts from the
  // site engagement service), a threshold on the minimum site engagement score
  // does not disqualify |this| from requesting hints for any host.
  pref_service_->SetDouble(
      optimization_guide::prefs::
          kHintsFetcherTopHostBlocklistMinimumEngagementScore,
      -1.0f);

  for (const auto& detail : engagement_details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS())
      continue;
    if (top_host_blocklist->size() >=
        optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize()) {
      // Set the minimum engagement score to the score of the host that
      // could not be added to  |top_host_blocklist|. Add a small epsilon value
      // to the threshold so that any host with score equal to or less than
      // the threshold is not included in the hints fetcher request.
      pref_service_->SetDouble(
          optimization_guide::prefs::
              kHintsFetcherTopHostBlocklistMinimumEngagementScore,
          std::min(detail.total_score + 0.001f,
                   optimization_guide::features::
                       MinTopHostEngagementScoreThreshold()));
      break;
    }
    top_host_blocklist->SetBoolKey(
        optimization_guide::HashHostForDictionary(detail.origin.host()), true);
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "OptimizationGuide.HintsFetcher.TopHostProvider.BlocklistSize."
      "OnInitialize",
      top_host_blocklist->size());

  pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlocklist,
                     *top_host_blocklist);

  UpdateCurrentBlocklistState(
      pref_service_, optimization_guide::prefs::
                         HintsFetcherTopHostBlocklistState::kInitialized);
}

// static
void OptimizationGuideTopHostProvider::MaybeUpdateTopHostBlocklist(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  // Do not update the top host list if the profile is off the record.
  if (profile->IsOffTheRecord())
    return;

  PrefService* pref_service = profile->GetPrefs();

  bool is_user_permitted_to_fetch_hints =
      optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), pref_service);
  if (!is_user_permitted_to_fetch_hints) {
    // User toggled state during the session. Make sure the blocklist is
    // cleared.
    ResetTopHostBlocklistState(pref_service);
    return;
  }
  DCHECK(is_user_permitted_to_fetch_hints);

  // Only proceed to update the blocklist if we have a blocklist to update.
  if (GetCurrentBlocklistState(pref_service) !=
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
          kInitialized) {
    return;
  }

  DictionaryPrefUpdate blocklist_pref(
      pref_service, optimization_guide::prefs::kHintsFetcherTopHostBlocklist);
  if (!blocklist_pref->FindKey(optimization_guide::HashHostForDictionary(
          navigation_handle->GetURL().host()))) {
    return;
  }
  blocklist_pref->RemovePath(optimization_guide::HashHostForDictionary(
      navigation_handle->GetURL().host()));
  if (blocklist_pref->empty()) {
    blocklist_pref->Clear();
    pref_service->SetInteger(
        optimization_guide::prefs::kHintsFetcherTopHostBlocklistState,
        static_cast<int>(optimization_guide::prefs::
                             HintsFetcherTopHostBlocklistState::kEmpty));
  }
}

std::vector<std::string> OptimizationGuideTopHostProvider::GetTopHosts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK(pref_service_);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // The user toggled state during the session. Return empty.
  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), pref_service_))
    return std::vector<std::string>();

  // It's possible that the blocklist is initialized but
  // kTimeHintsFetcherTopHostBlocklistLastInitialized pref is not populated.
  // This may happen since the logic to populate
  // kTimeHintsFetcherTopHostBlocklistLastInitialized pref was added in a later
  // Chrome version. In that case, set
  // kTimeHintsFetcherTopHostBlocklistLastInitialized to the conservative value
  // of current time.
  if (pref_service_->GetDouble(
          optimization_guide::prefs::
              kTimeHintsFetcherTopHostBlocklistLastInitialized) == 0) {
    pref_service_->SetDouble(
        optimization_guide::prefs::
            kTimeHintsFetcherTopHostBlocklistLastInitialized,
        time_clock_->Now().ToDeltaSinceWindowsEpoch().InSecondsF());
  }

  if (GetCurrentBlocklistState(pref_service_) ==
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
          kNotInitialized) {
    InitializeHintsFetcherTopHostBlocklist();
    return std::vector<std::string>();
  }

  // Create SiteEngagementService to request site engagement scores.
  auto* engagement_service =
      site_engagement::SiteEngagementService::Get(profile);

  const base::DictionaryValue* top_host_blocklist = nullptr;
  if (GetCurrentBlocklistState(pref_service_) !=
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::kEmpty) {
    top_host_blocklist = pref_service_->GetDictionary(
        optimization_guide::prefs::kHintsFetcherTopHostBlocklist);
    UMA_HISTOGRAM_COUNTS_1000(
        "OptimizationGuide.HintsFetcher.TopHostProvider.BlocklistSize."
        "OnRequest",
        top_host_blocklist->size());
    // This check likely should not be needed as the process of removing hosts
    // from the blocklist should check and update the pref state.
    if (top_host_blocklist->size() == 0) {
      UpdateCurrentBlocklistState(
          pref_service_,
          optimization_guide::prefs::HintsFetcherTopHostBlocklistState::kEmpty);
      top_host_blocklist = nullptr;
    }
  }

  std::vector<std::string> top_hosts;

  // Create a vector of the top hosts by engagement score up to |max_sites|
  // size. Currently utilizes just the first |max_sites| entries. Only HTTPS
  // schemed hosts are included. Hosts are filtered by the blocklist that is
  // populated when DataSaver is first enabled.
  std::vector<site_engagement::mojom::SiteEngagementDetails>
      engagement_details = engagement_service->GetAllDetails();

  std::sort(engagement_details.begin(), engagement_details.end(),
            [](const site_engagement::mojom::SiteEngagementDetails& lhs,
               const site_engagement::mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score > rhs.total_score;
            });

  base::Time blocklist_initialized_time =
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSecondsD(pref_service_->GetDouble(
              optimization_guide::prefs::
                  kTimeHintsFetcherTopHostBlocklistLastInitialized)));

  base::TimeDelta duration_since_blocklist_initialized =
      (time_clock_->Now() - blocklist_initialized_time);

  for (const auto& detail : engagement_details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS())
      continue;
    // Once the engagement score is less than the initial engagement score for a
    // newly navigated host, return the current set of top hosts. This threshold
    // prevents hosts that have not been engaged recently from having hints
    // requested for them. The engagement_details are sorted above in descending
    // order by engagement score.
    // This filtering is applied only if the the blocklist was initialized
    // recently. If the blocklist was initialized too far back in time, hosts
    // that could not make it to blocklist should have either been navigated to
    // or would have fallen off the blocklist.
    if (duration_since_blocklist_initialized <=
            optimization_guide::features::
                DurationApplyLowEngagementScoreThreshold() &&
        detail.total_score <
            pref_service_->GetDouble(
                optimization_guide::prefs::
                    kHintsFetcherTopHostBlocklistMinimumEngagementScore)) {
      return top_hosts;
    }
    if (!IsHostBlocklisted(top_host_blocklist, detail.origin.host())) {
      top_hosts.push_back(detail.origin.host());
    }
  }

  return top_hosts;
}
