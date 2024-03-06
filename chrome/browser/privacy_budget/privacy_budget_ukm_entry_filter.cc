// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"

#include <algorithm>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

PrivacyBudgetUkmEntryFilter::PrivacyBudgetUkmEntryFilter(
    IdentifiabilityStudyState* state)
    : identifiability_study_state_(state) {}

bool PrivacyBudgetUkmEntryFilter::FilterEntry(
    ukm::mojom::UkmEntry* entry,
    base::flat_set<uint64_t>* removed_metric_hashes) {
  // We don't yet deal with any event other than Identifiability. All other
  // types of events pass through.
  if (entry->event_hash != ukm::builders::Identifiability::kEntryNameHash)
    return true;

  const bool enabled = blink::IdentifiabilityStudySettings::Get()->IsActive();

  // If the study is not enabled, drop all identifiability events.
  if (!enabled || entry->metrics.empty())
    return false;

  // Contains newly encountered surfaces in entry->metrics.
  std::vector<blink::IdentifiableSurface> encountered_surfaces;
  encountered_surfaces.reserve(entry->metrics.size());

  base::EraseIf(entry->metrics, [&](auto metric) {
    const auto surface =
        blink::IdentifiableSurface::FromMetricHash(metric.first);
    const blink::IdentifiableToken token = metric.second;

    if (!blink::IdentifiabilityStudySettings::Get()->ShouldSampleSurface(
            surface))
      return true;

    if (identifiability_study_state_->ShouldReportEncounteredSurface(
            entry->source_id, surface)) {
      encountered_surfaces.push_back(surface);
    }

    return !identifiability_study_state_->ShouldRecordSurface(surface);
  });

  uint64_t index = 0;
  for (const auto& v : encountered_surfaces) {
    blink::IdentifiableSurface s = blink::IdentifiableSurface::FromTypeAndToken(
        blink::IdentifiableSurface::Type::kMeasuredSurface, index++);
    entry->metrics.insert_or_assign(entry->metrics.end(), s.ToUkmMetricHash(),
                                    v.ToUkmMetricHash());
  }

  if (!metadata_reported_) {
    entry->metrics.insert_or_assign(
        ukm::builders::Identifiability::kStudyGeneration_626NameHash,
        identifiability_study_state_->generation());
    entry->metrics.insert_or_assign(
        ukm::builders::Identifiability::kGeneratorVersion_926NameHash,
        IdentifiabilityStudyState::kGeneratorVersion);
    metadata_reported_ = true;
  }

  // Identifiability metrics can leak information simply by being measured.
  // Hence the metrics that are filtered out aren't returned in
  // |removed_metric_hashes|.
  return !entry->metrics.empty();
}

void PrivacyBudgetUkmEntryFilter::OnStoreRecordingsInReport() {
  identifiability_study_state_->ResetPerReportState();
  metadata_reported_ = false;
}
