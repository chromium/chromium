// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sample_metadata.h"

#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

SampleMetadata::SampleMetadata(StringPiece name)
    : name_hash_(HashMetricName(name)) {}

void SampleMetadata::Set(int64_t value) {
  GetSampleMetadataRecorder()->Set(name_hash_, absl::nullopt, value);
}

void SampleMetadata::Set(int64_t key, int64_t value) {
  GetSampleMetadataRecorder()->Set(name_hash_, key, value);
}

void SampleMetadata::Remove() {
  GetSampleMetadataRecorder()->Remove(name_hash_, absl::nullopt);
}

void SampleMetadata::Remove(int64_t key) {
  GetSampleMetadataRecorder()->Remove(name_hash_, key);
}

ScopedSampleMetadata::ScopedSampleMetadata(StringPiece name, int64_t value)
    : name_hash_(HashMetricName(name)) {
  GetSampleMetadataRecorder()->Set(name_hash_, absl::nullopt, value);
}

ScopedSampleMetadata::ScopedSampleMetadata(StringPiece name,
                                           int64_t key,
                                           int64_t value)
    : name_hash_(HashMetricName(name)), key_(key) {
  GetSampleMetadataRecorder()->Set(name_hash_, key, value);
}

ScopedSampleMetadata::~ScopedSampleMetadata() {
  GetSampleMetadataRecorder()->Remove(name_hash_, key_);
}

// This function is friended by StackSamplingProfiler so must live directly in
// the base namespace.
void ApplyMetadataToPastSamplesImpl(TimeTicks period_start,
                                    TimeTicks period_end,
                                    int64_t name_hash,
                                    absl::optional<int64_t> key,
                                    int64_t value) {
  StackSamplingProfiler::ApplyMetadataToPastSamples(period_start, period_end,
                                                    name_hash, key, value);
}

void ApplyMetadataToPastSamples(TimeTicks period_start,
                                TimeTicks period_end,
                                StringPiece name,
                                int64_t value) {
  return ApplyMetadataToPastSamplesImpl(
      period_start, period_end, HashMetricName(name), absl::nullopt, value);
}

void ApplyMetadataToPastSamples(TimeTicks period_start,
                                TimeTicks period_end,
                                StringPiece name,
                                int64_t key,
                                int64_t value) {
  return ApplyMetadataToPastSamplesImpl(period_start, period_end,
                                        HashMetricName(name), key, value);
}

MetadataRecorder* GetSampleMetadataRecorder() {
  static NoDestructor<MetadataRecorder> instance;
  return instance.get();
}

}  // namespace base
