// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_delta_serialization.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/statistics_recorder.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/values.h"

namespace base {

namespace {

// Create or find existing histogram and add the samples from pickle.
// Silently returns when seeing any data problem in the pickle.
void DeserializeHistogramAndAddSamples(PickleIterator* iter) {
  HistogramBase* histogram = DeserializeHistogramInfo(iter);
  if (!histogram) {
    return;
  }

  if (histogram->HasFlags(HistogramBase::kIPCSerializationSourceFlag)) {
    DVLOG(1) << "Single process mode, histogram observed and not copied: "
             << histogram->histogram_name();
    return;
  }
  histogram->AddSamplesFromPickle(iter);
}

}  // namespace

HistogramDeltaSerialization::~HistogramDeltaSerialization() = default;

void HistogramDeltaSerialization::PrepareAndSerializeDeltas(
    std::vector<std::string>* serialized_deltas,
    bool include_persistent) {
  DCHECK(thread_checker_.CalledOnValidThread());

  serialized_deltas_ = serialized_deltas;
  // Note: Before serializing, we set the kIPCSerializationSourceFlag for all
  // the histograms, so that the receiving process can distinguish them from the
  // local histograms.
  StatisticsRecorder::PrepareDeltas(include_persistent,
                                    Histogram::kIPCSerializationSourceFlag,
                                    Histogram::kNoFlags, this);
  serialized_deltas_ = nullptr;
}

// static
void HistogramDeltaSerialization::DeserializeAndAddSamples(
    const std::vector<std::string>& serialized_deltas) {
  for (const std::string& serialized_delta : serialized_deltas) {
    PickleIterator iter =
        PickleIterator::WithData(as_byte_span(serialized_delta));
    DeserializeHistogramAndAddSamples(&iter);
  }
}

void HistogramDeltaSerialization::RecordDelta(
    const HistogramBase& histogram,
    const HistogramSamples& snapshot) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(0, snapshot.TotalCount());

  Pickle pickle;
  histogram.SerializeInfo(&pickle);
  snapshot.Serialize(&pickle);
  serialized_deltas_->emplace_back(pickle.AsStringView());
}

}  // namespace base
