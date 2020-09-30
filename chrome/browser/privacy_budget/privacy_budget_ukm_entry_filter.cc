// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"

#include <algorithm>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_budget/sampled_surface_tracker.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

PrivacyBudgetUkmEntryFilter::PrivacyBudgetUkmEntryFilter(
    IdentifiabilityStudyState* state)
    : identifiability_study_state_(state) {}

bool PrivacyBudgetUkmEntryFilter::FilterEntry(
    ukm::mojom::UkmEntry* entry,
    base::flat_set<uint64_t>* removed_metric_hashes) const {
  // We don't yet deal with any event other than Identifiability. All other
  // types of events pass through.
  if (entry->event_hash != ukm::builders::Identifiability::kEntryNameHash)
    return true;

  const bool enabled = blink::IdentifiabilityStudySettings::Get()->IsActive();

  // If the study is not enabled, drop all identifiability events.
  if (!enabled || entry->metrics.empty())
    return false;

  std::vector<blink::IdentifiableSurface> sampled_surfaces;
  sampled_surfaces.reserve(entry->metrics.size());
  base::EraseIf(entry->metrics, [&](auto metric) {
    const auto surface =
        blink::IdentifiableSurface::FromMetricHash(metric.first);
    // Exclude the set that are blocked from all measurements.
    if (!blink::IdentifiabilityStudySettings::Get()->IsSurfaceAllowed(surface))
      return true;

    // Record the set of surfaces sampled by the site.
    if (identifiability_study_state_->ShouldRecordSurface(entry->source_id,
                                                          surface))
      sampled_surfaces.push_back(surface);

    // Exclude the set that are disabled for this user
    return !identifiability_study_state_->ShouldSampleSurface(surface);
  });

  uint64_t sample_idx = 0;
  for (const auto& v : sampled_surfaces) {
    // Add entries marking the surfaces that were sampled by the source as
    // sampled.
    blink::IdentifiableSurface s = blink::IdentifiableSurface::FromTypeAndToken(
        blink::IdentifiableSurface::Type::kMeasuredSurface, sample_idx++);
    entry->metrics.insert_or_assign(entry->metrics.end(), s.ToUkmMetricHash(),
                                    v.ToUkmMetricHash());
  }

  // Identifiability metrics can leak information simply by being measured.
  // Hence the metrics that are filtered out aren't returning in
  // |removed_metric_hashes|.
  return !entry->metrics.empty();
}

void PrivacyBudgetUkmEntryFilter::OnStoreRecordingsInReport() const {
  identifiability_study_state_->ResetRecordedSurfaces();
}
