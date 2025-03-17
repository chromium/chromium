// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace {

void LogNudgeInteractionHistogram(
    contextual_cueing::NudgeInteraction interaction) {
  base::UmaHistogramEnumeration("ContextualCueing.NudgeInteraction",
                                interaction);
}

void LogNudgeInteractionUKM(ukm::SourceId source_id,
                            contextual_cueing::NudgeInteraction interaction,
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

}  // namespace

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService(
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service)
    : recent_nudge_tracker_(kNudgeCapCount.Get(), kNudgeCapTime.Get()),
      recent_visited_origins_(kVisitedDomainsLimit.Get()),
      page_content_extraction_service_(page_content_extraction_service) {
  CHECK(base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing));

  if (kEnablePageContentExtraction.Get()) {
    page_content_extraction_service_->AddObserver(this);
  }
}

ContextualCueingService::~ContextualCueingService() {
  if (kEnablePageContentExtraction.Get()) {
    page_content_extraction_service_->RemoveObserver(this);
  }
}

void ContextualCueingService::ReportPageLoad() {
  if (remaining_quiet_loads_) {
    remaining_quiet_loads_--;
  }
}

void ContextualCueingService::CueingNudgeShown(const GURL& url) {
  recent_nudge_tracker_.CueingNudgeShown();

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

  base::TimeDelta backoff_duration =
      kBackoffTime.Get() * pow(kBackoffMultiplierBase.Get(), dismiss_count_);

  backoff_end_time_ = base::Time::Now() + backoff_duration;
  ++dismiss_count_;
}

void ContextualCueingService::CueingNudgeClicked() {

  dismiss_count_ = 0;
}

NudgeDecision ContextualCueingService::CanShowNudge(const GURL& url) {
  if (remaining_quiet_loads_ > 0) {
    return NudgeDecision::kNotEnoughPageLoadsSinceLastNudge;
  }
  if (IsNudgeBlockedByBackoffRule()) {
    return NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge;
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
  return backoff_end_time_ && (base::Time::Now() < backoff_end_time_);
}

void ContextualCueingService::OnNudgeActivity(
    const GURL& url,
    ukm::SourceId source_id,
    base::TimeTicks document_available_time,
    tabs::GlicNudgeActivity activity) {
  std::optional<base::TimeTicks> nudge_time =
      recent_nudge_tracker_.GetMostRecentNudgeTime();
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
    case tabs::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
      interaction = NudgeInteraction::kIgnoredTabChange;
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
    LogNudgeInteractionUKM(source_id, interaction, document_available_time,
                           *nudge_time);
  }
}

void ContextualCueingService::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  auto* cueing_page_data = ContextualCueingPageData::GetForPage(page);
  if (!cueing_page_data) {
    return;
  }
  cueing_page_data->OnPageContentExtracted(page_content);
}

}  // namespace contextual_cueing
