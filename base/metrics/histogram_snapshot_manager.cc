// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_snapshot_manager.h"

#include <memory>

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_samples.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"

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
  if (samples.TotalCount() <= 0) {
    return;
  }

  // Crash if we detect that our histograms have been overwritten.  This may be
  // a fair distance from the memory smasher, but we hope to correlate these
  // crashes with other events, such as plugins, or usage patterns, etc.
  uint32_t corruption = histogram->FindCorruption(samples);
  base::debug::Alias(&corruption);
  if (HistogramBase::BUCKET_ORDER_ERROR & corruption) {
    // Extract fields useful during debug.
    const BucketRanges* ranges =
        static_cast<const Histogram*>(histogram)->bucket_ranges();
    uint32_t ranges_checksum = ranges->checksum();
    uint32_t ranges_calc_checksum = ranges->CalculateChecksum();
    int32_t flags = histogram->flags();
    // Ensure that compiler keeps around pointers to |histogram| and its
    // internal |bucket_ranges_| for any minidumps.
    base::debug::Alias(&ranges_checksum);
    base::debug::Alias(&ranges_calc_checksum);
    base::debug::Alias(&flags);

    // TODO(crbug.com/397733765): Clean up crash keys once the bug is fixed.
    SCOPED_CRASH_KEY_STRING32("PrepareSamples", "histogram",
                              histogram->histogram_name());
    std::string ranges_string;
    for (size_t index = 0; index < ranges->size(); ++index) {
      ranges_string += base::StringPrintf("%d ", ranges->range(index));
    }
    SCOPED_CRASH_KEY_STRING32("PrepareSamples", "ranges", ranges_string);

    // The checksum should have caught this, so crash separately if it didn't.
    CHECK_NE(0U, HistogramBase::RANGE_CHECKSUM_ERROR & corruption);
    NOTREACHED();  // Crash for the bucket order corruption.
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

  histogram_flattener_->RecordDelta(*histogram, samples);
}

}  // namespace base
