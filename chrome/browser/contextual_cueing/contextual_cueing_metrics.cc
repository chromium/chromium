// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace contextual_cueing {

namespace {

std::string InteractionTypeToString(ContextualCueingInteraction interaction) {
  switch (interaction) {
    case ContextualCueingInteraction::kCueClicked:
      return "Clicked";
    case ContextualCueingInteraction::kCueDismissed:
      return "Dismissed";
    case ContextualCueingInteraction::kCueEditPrompt:
      return "EditPrompt";
    case ContextualCueingInteraction::kCueSuggestionsSettings:
      return "Settings";
  }
}

int BucketTabCount(int raw_count) {
  return ukm::GetExponentialBucketMin(raw_count, 1.5);
}

}  // namespace

void RecordCueShownMetrics(ukm::SourceId source_id,
                           std::string_view cuj,
                           const CueTabMetrics& tab_metrics,
                           base::TimeDelta latency) {
  base::UmaHistogramSparse("ContextualCueing.V2.CueShown",
                           base::HashMetricName(cuj));
  base::UmaHistogramTimes("ContextualCueing.V2.CueShownLatency", latency);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::ContextualCueing_CueShown(source_id)
      .SetSuggestedCujCategory(base::HashMetricName(cuj))
      .SetMatchedTabCount(BucketTabCount(tab_metrics.matched_count))
      .SetMissingTabCount(BucketTabCount(tab_metrics.missing_count))
      .SetNavigatedAwayTabCount(
          BucketTabCount(tab_metrics.navigated_away_count))
      .SetProactiveCueLatencyAfterPageLoad(latency.InMilliseconds())
      .SetProactiveCueDecision(
          static_cast<int64_t>(ContextualCueingDecision::kSuccess))
      .Record(ukm_recorder);
}

void RecordContextualCueingInteraction(
    ContextualCueingInteraction contextual_cueing_interaction,
    const std::string& cuj,
    ukm::SourceId source_id,
    base::TimeDelta shown_duration) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueInteraction",
                                contextual_cueing_interaction);

  std::string histogram_name =
      "ContextualCueing.V2.CueInteraction." +
      InteractionTypeToString(contextual_cueing_interaction);
  base::UmaHistogramSparse(histogram_name, base::HashMetricName(cuj));

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::ContextualCueing_CueInteraction(source_id)
      .SetProactiveCueShownDurationMs(ukm::GetExponentialBucketMinForUserTiming(
          shown_duration.InMilliseconds()))
      .SetProactiveCueInteraction(
          static_cast<int64_t>(contextual_cueing_interaction))
      .Record(ukm_recorder);
}

void RecordContextualCueingDecision(
    ukm::SourceId source_id,
    ContextualCueingDecision contextual_cueing_decision) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.Decision",
                                contextual_cueing_decision);

  // If the decision is kSuccess, RecordCueShownMetrics will record the
  // ProactiveCueDecision UKM instead.
  if (contextual_cueing_decision != ContextualCueingDecision::kSuccess) {
    auto* ukm_recorder = ukm::UkmRecorder::Get();
    ukm::builders::ContextualCueing_CueShown(source_id)
        .SetProactiveCueDecision(
            static_cast<int64_t>(contextual_cueing_decision))
        .Record(ukm_recorder);
  }
}

void RecordCueFormFactorShown(CueFormFactor form_factor) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueFormFactor.Shown",
                                form_factor);
}

void RecordCueFormFactorHidden(CueFormFactor form_factor) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueFormFactor.Hidden",
                                form_factor);
}

void RecordChipClickedCollapsedDuration(base::TimeDelta collapsed_duration) {
  base::UmaHistogramLongTimes(
      "ContextualCueing.V2.ChipClicked.CollapsedDuration", collapsed_duration);
}

}  // namespace contextual_cueing
