// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_snapshot_manager.h"

#include <memory>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_samples.h"

namespace base {

HistogramSnapshotManager::HistogramSnapshotManager(
    HistogramFlattener* histogram_flattener)
    : histogram_flattener_(histogram_flattener) {
  DCHECK(histogram_flattener_);
}

HistogramSnapshotManager::~HistogramSnapshotManager() = default;

void HistogramSnapshotManager::PrepareDeltas(
    const std::vector<HistogramBase*>& histograms,
    HistogramBase::Flags flags_to_set,
    HistogramBase::Flags required_flags) {
  for (HistogramBase* const histogram : histograms) {
    histogram->SetFlags(flags_to_set);
    if (histogram->HasFlags(required_flags)) {
      PrepareDelta(histogram);
    }
  }
}

void HistogramSnapshotManager::SnapshotUnloggedSamples(
    const std::vector<HistogramBase*>& histograms,
    HistogramBase::Flags required_flags) {
  DCHECK(!unlogged_samples_snapshot_taken_);
  unlogged_samples_snapshot_taken_ = true;
  for (HistogramBase* const histogram : histograms) {
    if (histogram->HasFlags(required_flags)) {
      const HistogramSnapshotPair& histogram_snapshot_pair =
          histograms_and_snapshots_.emplace_back(
              histogram, histogram->SnapshotUnloggedSamples());
      PrepareSamples(histogram_snapshot_pair.first,
                     *histogram_snapshot_pair.second);
    }
  }
}

void HistogramSnapshotManager::MarkUnloggedSamplesAsLogged() {
  DCHECK(unlogged_samples_snapshot_taken_);
  unlogged_samples_snapshot_taken_ = false;
  std::vector<HistogramSnapshotPair> histograms_and_snapshots;
  histograms_and_snapshots.swap(histograms_and_snapshots_);
  for (auto& [histogram, snapshot] : histograms_and_snapshots) {
    histogram->MarkSamplesAsLogged(*snapshot);
  }
}

void HistogramSnapshotManager::PrepareDelta(HistogramBase* histogram) {
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotDelta();
  PrepareSamples(histogram, *samples);
}

void HistogramSnapshotManager::PrepareFinalDelta(
    const HistogramBase* histogram) {
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotFinalDelta();
  PrepareSamples(histogram, *samples);
}

void HistogramSnapshotManager::PrepareSamples(const HistogramBase* histogram,
                                              const HistogramSamples& samples) {
  DCHECK(histogram_flattener_);

  // Crash if we detect that our histograms have been overwritten.  This may be
  // a fair distance from the memory smasher, but we hope to correlate these
  // crashes with other events, such as plugins, or usage patterns, etc.
  uint32_t corruption = histogram->FindCorruption(samples);
  if (HistogramBase::BUCKET_ORDER_ERROR & corruption) {
    // Extract fields useful during debug.
    const BucketRanges* ranges =
        static_cast<const Histogram*>(histogram)->bucket_ranges();
    uint32_t ranges_checksum = ranges->checksum();
    uint32_t ranges_calc_checksum = ranges->CalculateChecksum();
    int32_t flags = histogram->flags();
    // The checksum should have caught this, so crash separately if it didn't.
    CHECK_NE(0U, HistogramBase::RANGE_CHECKSUM_ERROR & corruption);
    CHECK(false);  // Crash for the bucket order corruption.
    // Ensure that compiler keeps around pointers to |histogram| and its
    // internal |bucket_ranges_| for any minidumps.
    base::debug::Alias(&ranges_checksum);
    base::debug::Alias(&ranges_calc_checksum);
    base::debug::Alias(&flags);
  }
  // Checksum corruption might not have caused order corruption.
  CHECK_EQ(0U, HistogramBase::RANGE_CHECKSUM_ERROR & corruption);

  // Note, at this point corruption can only be COUNT_HIGH_ERROR or
  // COUNT_LOW_ERROR and they never arise together, so we don't need to extract
  // bits from corruption.
  if (corruption) {
    DLOG(ERROR) << "Histogram: \"" << histogram->histogram_name()
                << "\" has data corruption: " << corruption;
    // Don't record corrupt data to metrics services.
    return;
  }

  if (samples.TotalCount() > 0)
    histogram_flattener_->RecordDelta(*histogram, samples);
}

}  // namespace base
