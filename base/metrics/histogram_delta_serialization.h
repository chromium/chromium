// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_DELTA_SERIALIZATION_H_
#define BASE_METRICS_HISTOGRAM_DELTA_SERIALIZATION_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/threading/thread_checker.h"

namespace base {

class HistogramBase;

// Serializes and restores histograms deltas.
class BASE_EXPORT HistogramDeltaSerialization
    : public HistogramSnapshotManager {
 public:
  HistogramDeltaSerialization() = default;
  HistogramDeltaSerialization(const HistogramDeltaSerialization&) = delete;
  HistogramDeltaSerialization& operator=(const HistogramDeltaSerialization&) =
      delete;

  ~HistogramDeltaSerialization() override;

  // Computes deltas in histogram bucket counts relative to the previous call to
  // this method. Stores the deltas in serialized form into |serialized_deltas|.
  // If |serialized_deltas| is null, no data is serialized, though the next call
  // will compute the deltas relative to this one. Setting |include_persistent|
  // will include histograms held in persistent memory (and thus may be reported
  // elsewhere); otherwise only histograms local to this process are serialized.
  void PrepareAndSerializeDeltas(std::vector<std::string>* serialized_deltas,
                                 bool include_persistent);

  // Deserialize deltas and add samples to corresponding histograms, creating
  // them if necessary. Silently ignores errors in |serialized_deltas|.
  // The |mapper| callback is invoked for each serialized histogram. It allows
  // the caller to conditionally rename or drop histograms before they are
  // merged. Returning an empty string indicates that the histogram should be
  // dropped. This is particularly useful where the caller needs to separate
  // metrics without the child process's knowledge.
  static void DeserializeAndAddSamples(
      const std::vector<std::string>& serialized_deltas,
      HistogramBase::NameMapper mapper);

 private:
  // HistogramSnapshotManager implementation.
  void RecordDelta(const HistogramBase& histogram,
                   const HistogramSamples& snapshot) override;

  ThreadChecker thread_checker_;

  // Output buffer for serialized deltas.
  raw_ptr<std::vector<std::string>> serialized_deltas_;
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_DELTA_SERIALIZATION_H_
