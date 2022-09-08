// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_
#define BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_

#include <stdint.h>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"

namespace base {

class HistogramSamples;
class HistogramFlattener;

// HistogramSnapshotManager handles the logistics of gathering up available
// histograms for recording either to disk or for transmission (such as from
// renderer to browser, or from browser to UMA upload). Since histograms can sit
// in memory for an extended period of time, and are vulnerable to memory
// corruption, this class also validates as much redundancy as it can before
// calling for the marginal change (a.k.a., delta) in a histogram to be
// recorded.
class BASE_EXPORT HistogramSnapshotManager final {
 public:
  explicit HistogramSnapshotManager(HistogramFlattener* histogram_flattener);

  HistogramSnapshotManager(const HistogramSnapshotManager&) = delete;
  HistogramSnapshotManager& operator=(const HistogramSnapshotManager&) = delete;

  ~HistogramSnapshotManager();

  // Snapshots all histograms and asks |histogram_flattener_| to record the
  // delta. |flags_to_set| is used to set flags for each histogram.
  // |required_flags| is used to select which histograms to record. Only
  // histograms with all of the required flags are selected. If all histograms
  // should be recorded, use |Histogram::kNoFlags| as the required flag.
  void PrepareDeltas(const std::vector<HistogramBase*>& histograms,
                     HistogramBase::Flags flags_to_set,
                     HistogramBase::Flags required_flags);

  // When the collection is not so simple as can be done using a single
  // iterator, the steps can be performed separately. Call PerpareDelta()
  // as many times as necessary. PrepareFinalDelta() works like PrepareDelta()
  // except that it does not update the previous logged values and can thus
  // be used with read-only files.
  void PrepareDelta(HistogramBase* histogram);
  void PrepareFinalDelta(const HistogramBase* histogram);

 private:
  FRIEND_TEST_ALL_PREFIXES(HistogramSnapshotManagerTest, CheckMerge);

  // During a snapshot, samples are acquired and aggregated. This structure
  // contains all the information for a given histogram that persists between
  // collections.
  struct SampleInfo {
    // The set of inconsistencies (flags) already seen for the histogram.
    // See HistogramBase::Inconsistency for values.
    uint32_t inconsistencies = 0;
  };

  // Capture and hold samples from a histogram. This does all the heavy
  // lifting for PrepareDelta() and PrepareAbsolute().
  void PrepareSamples(const HistogramBase* histogram,
                      std::unique_ptr<HistogramSamples> samples);

  // |histogram_flattener_| handles the logistics of recording the histogram
  // deltas.
  const raw_ptr<HistogramFlattener> histogram_flattener_;  // Weak.

  // For histograms, track what has been previously seen, indexed
  // by the hash of the histogram name.
  std::map<uint64_t, SampleInfo> known_histograms_;

  // A flag indicating if a thread is currently doing an operation. This is
  // used to check against concurrent access which is not supported. A Thread-
  // Checker is not sufficient because it may be guarded by at outside lock
  // (as is the case with cronet).
  std::atomic<bool> is_active_;
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_SNAPSHOT_MANAGER_H_
