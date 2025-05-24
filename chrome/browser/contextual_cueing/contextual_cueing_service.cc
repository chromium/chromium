// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/common/buildflags.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_anonymization_key.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

namespace contextual_cueing {
namespace {

void LogNudgeInteractionHistogram(NudgeInteraction interaction) {
  base::UmaHistogramEnumeration("ContextualCueing.NudgeInteraction",
                                interaction);
}

void LogNudgeInteractionUKM(ukm::SourceId source_id,
                            NudgeInteraction interaction,
                            base::TimeTicks document_available_time,
                            base::TimeTicks nudge_shown_time) {
  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::ContextualCueing_NudgeInteraction(source_id)
      .SetNudgeInteraction(static_cast<int64_t>(interaction))
      .SetNudgeShownDuration(ukm::GetExponentialBucketMinForUserTiming(
          (base::TimeTicks::Now() - nudge_shown_time).InMilliseconds()))
      .SetNudgeLatencyAfterPageLoad(
          (nudge_shown_time - document_available_time).InMilliseconds())
      .Record(ukm_recorder->Get());
}

#if BUILDFLAG(ENABLE_GLIC)
bool IsGlicTabContextEnabled(PrefService* pref_service) {
  return pref_service->GetBoolean(glic::prefs::kGlicTabContextEnabled);
}
#endif

}  // namespace

ContextualCueingService::ContextualCueingService(
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    predictors::LoadingPredictor* loading_predictor,
    PrefService* pref_service,
    TemplateURLService* template_url_service)
    : recent_nudge_tracker_(kNudgeCapCount.Get(), kNudgeCapTime.Get()),
      recent_visited_origins_(kVisitedDomainsLimit.Get()),
      page_content_extraction_service_(page_content_extraction_service),
      optimization_guide_keyed_service_(optimization_guide_keyed_service),
      loading_predictor_(loading_predictor),
      pref_service_(pref_service),
      template_url_service_(template_url_service),
      mes_url_(optimization_guide::switches::GetModelExecutionServiceURL()) {
  CHECK(base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing) ||
        base::FeatureList::IsEnabled(
            contextual_cueing::kGlicZeroStateSuggestions));
  if (optimization_guide_keyed_service_ &&
      base::FeatureList::IsEnabled(
          contextual_cueing::kGlicZeroStateSuggestions)) {
    optimization_guide_keyed_service_->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS});
  }

  if (kEnablePageContentExtraction.Get() && page_content_extraction_service_) {
    page_content_extraction_service_->AddObserver(this);
  }
}

ContextualCueingService::~ContextualCueingService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (kEnablePageContentExtraction.Get() && page_content_extraction_service_) {
    page_content_extraction_service_->RemoveObserver(this);
  }
}

void ContextualCueingService::ReportPageLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remaining_quiet_loads_) {
    remaining_quiet_loads_--;
  }
}

void ContextualCueingService::CueingNudgeShown(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  recent_nudge_tracker_.CueingNudgeShown();
  shown_backoff_end_time_ =
      base::TimeTicks::Now() + kMinTimeBetweenNudges.Get();

  if (kMinPageCountBetweenNudges.Get()) {
    // Let the cue logic be performed the next page after quiet count pages.
    remaining_quiet_loads_ = kMinPageCountBetweenNudges.Get() + 1;
  }

  auto origin = url::Origin::Create(url);
  auto iter = recent_visited_origins_.Get(origin);
  if (iter == recent_visited_origins_.end()) {
    iter = recent_visited_origins_.Put(
        origin, NudgeCapTracker(kNudgeCapCountPerDomain.Get(),
                                kNudgeCapTimePerDomain.Get()));
  }
  iter->second.CueingNudgeShown();
}

void ContextualCueingService::CueingNudgeDismissed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta backoff_duration =
      kBackoffTime.Get() * pow(kBackoffMultiplierBase.Get(), dismiss_count_);

  dismiss_backoff_end_time_ = base::TimeTicks::Now() + backoff_duration;
  ++dismiss_count_;
}

void ContextualCueingService::CueingNudgeClicked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dismiss_count_ = 0;
}

NudgeDecision ContextualCueingService::CanShowNudge(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remaining_quiet_loads_ > 0) {
    return NudgeDecision::kNotEnoughPageLoadsSinceLastNudge;
  }
  if (shown_backoff_end_time_ &&
      base::TimeTicks::Now() < shown_backoff_end_time_) {
    return NudgeDecision::kNotEnoughTimeSinceLastNudgeShown;
  }
  if (IsNudgeBlockedByBackoffRule()) {
    return NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed;
  }
  if (!recent_nudge_tracker_.CanShowNudge()) {
    return NudgeDecision::kTooManyNudgesShownToTheUser;
  }
  auto iter = recent_visited_origins_.Peek(url::Origin::Create(url));
  if (iter != recent_visited_origins_.end() && !iter->second.CanShowNudge()) {
    return NudgeDecision::kTooManyNudgesShownToTheUserForDomain;
  }
  return NudgeDecision::kSuccess;
}

bool ContextualCueingService::IsNudgeBlockedByBackoffRule() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return dismiss_backoff_end_time_ &&
         (base::TimeTicks::Now() < dismiss_backoff_end_time_);
}

bool ContextualCueingService::IsPageTypeEligibleForContextualSuggestions(
    GURL url) const {
  // Non-HTTP/HTTPS pages are not eligible.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Search results pages are not eligible.
  if (template_url_service_ &&
      template_url_service_->ExtractSearchMetadata(url)) {
    return false;
  }

  return true;
}

void ContextualCueingService::OnNudgeActivity(
    content::WebContents* web_contents,
    base::TimeTicks document_available_time,
    tabs::GlicNudgeActivity activity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::TimeTicks> nudge_time =
      recent_nudge_tracker_.GetMostRecentNudgeTime();
  const GURL& url = web_contents->GetLastCommittedURL();
  NudgeInteraction interaction;
  bool log_ukm = false;
  switch (activity) {
    case tabs::GlicNudgeActivity::kNudgeShown:
      interaction = NudgeInteraction::kShown;
      CueingNudgeShown(url);
      break;
    case tabs::GlicNudgeActivity::kNudgeClicked:
      CueingNudgeClicked();
      interaction = NudgeInteraction::kClicked;
      log_ukm = true;
      break;
    case tabs::GlicNudgeActivity::kNudgeDismissed:
      interaction = NudgeInteraction::kDismissed;
      CueingNudgeDismissed();
      log_ukm = true;
      break;
    case tabs::GlicNudgeActivity::kNudgeNotShownWebContents:
      interaction = NudgeInteraction::kNudgeNotShownWebContents;
      break;
    case tabs::GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      interaction = NudgeInteraction::kNudgeNotShownWindowCallToActionUI;
      break;
    case tabs::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
      interaction = NudgeInteraction::kIgnoredTabChange;
      // The ActiveTabChanged activity is called very aggresivly and there may
      // not be an actively shown nudge. We should only log this as an action if
      // there is a shown nudge is dismissed
      if (!nudge_time) {
        return;
      }
      log_ukm = true;
      break;
    case tabs::GlicNudgeActivity::kNudgeIgnoredNavigation:
      interaction = NudgeInteraction::kIgnoredNavigation;
      log_ukm = true;
      break;
  }
  LogNudgeInteractionHistogram(interaction);
  // As this function is called multiple times per nudge only some of the
  // activities result in a UKM call.
  if (log_ukm) {
    CHECK(nudge_time);
    LogNudgeInteractionUKM(
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(), interaction,
        document_available_time, *nudge_time);
  }
}

void ContextualCueingService::PrepareToFetchContextualGlicZeroStateSuggestions(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kGlicZeroStateSuggestions)) {
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (!IsPageTypeEligibleForContextualSuggestions(
          web_contents->GetLastCommittedURL())) {
    return;
  }

  if (!IsGlicTabContextEnabled(pref_service_)) {
    return;
  }

  // This call preflights grabbing the page content.
  ZeroStateSuggestionsPageData::CreateForPage(web_contents->GetPrimaryPage());

  if (loading_predictor_) {
    net::NetworkAnonymizationKey anonymization_key =
        net::NetworkAnonymizationKey::CreateSameSite(
            net::SchemefulSite(mes_url_));
    loading_predictor_->PreconnectURLIfAllowed(
        mes_url_, /*allow_credentials=*/true, anonymization_key);
  }
#endif
}

void ContextualCueingService::GetContextualGlicZeroStateSuggestions(
    content::WebContents* web_contents,
    bool is_fre,
    GlicSuggestionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kGlicZeroStateSuggestions)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (!IsPageTypeEligibleForContextualSuggestions(
          web_contents->GetLastCommittedURL())) {
    std::move(callback).Run(std::nullopt);
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (!IsGlicTabContextEnabled(pref_service_)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ZeroStateSuggestionsPageData* page_data =
      ZeroStateSuggestionsPageData::GetOrCreateForPage(
          web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(
      is_fre, base::BindOnce(&ContextualCueingService::OnSuggestionsReceived,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::TimeTicks::Now(), std::move(callback)));
#else
  std::move(callback).Run(std::nullopt);
#endif
}

void ContextualCueingService::OnSuggestionsReceived(
    base::TimeTicks fetch_begin_time,
    GlicSuggestionsCallback callback,
    std::optional<std::vector<std::string>> suggestions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramTimes(suggestions
                              ? "ContextualCueing.GlicSuggestions."
                                "SuggestionsFetchLatency.ValidSuggestions"
                              : "ContextualCueing.GlicSuggestions."
                                "SuggestionsFetchLatency.EmptySuggestions",
                          base::TimeTicks::Now() - fetch_begin_time);

  std::move(callback).Run(suggestions);
}

void ContextualCueingService::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* cueing_page_data = ContextualCueingPageData::GetForPage(page);
  if (!cueing_page_data) {
    return;
  }
  cueing_page_data->OnPageContentExtracted(page_content);
}

}  // namespace contextual_cueing
