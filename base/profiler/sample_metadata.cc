// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sample_metadata.h"

#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/threading/thread_local.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

namespace {

absl::optional<PlatformThreadId> GetPlatformThreadIdForScope(
    SampleMetadataScope scope) {
  if (scope == SampleMetadataScope::kProcess)
    return absl::nullopt;
  return PlatformThread::CurrentId();
}

}  // namespace

SampleMetadata::SampleMetadata(StringPiece name, SampleMetadataScope scope)
    : name_hash_(HashMetricName(name)), scope_(scope) {}

void SampleMetadata::Set(int64_t value) {
  GetSampleMetadataRecorder()->Set(name_hash_, absl::nullopt,
                                   GetPlatformThreadIdForScope(scope_), value);
}

void SampleMetadata::Set(int64_t key, int64_t value) {
  GetSampleMetadataRecorder()->Set(name_hash_, key,
                                   GetPlatformThreadIdForScope(scope_), value);
}

void SampleMetadata::Remove() {
  GetSampleMetadataRecorder()->Remove(name_hash_, absl::nullopt,
                                      GetPlatformThreadIdForScope(scope_));
}

void SampleMetadata::Remove(int64_t key) {
  GetSampleMetadataRecorder()->Remove(name_hash_, key,
                                      GetPlatformThreadIdForScope(scope_));
}

ScopedSampleMetadata::ScopedSampleMetadata(StringPiece name,
                                           int64_t value,
                                           SampleMetadataScope scope)
    : name_hash_(HashMetricName(name)),
      thread_id_(GetPlatformThreadIdForScope(scope)) {
  GetSampleMetadataRecorder()->Set(name_hash_, absl::nullopt, thread_id_,
                                   value);
}

ScopedSampleMetadata::ScopedSampleMetadata(StringPiece name,
                                           int64_t key,
                                           int64_t value,
                                           SampleMetadataScope scope)
    : name_hash_(HashMetricName(name)),
      key_(key),
      thread_id_(GetPlatformThreadIdForScope(scope)) {
  GetSampleMetadataRecorder()->Set(name_hash_, key, thread_id_, value);
}

ScopedSampleMetadata::~ScopedSampleMetadata() {
  GetSampleMetadataRecorder()->Remove(name_hash_, key_, thread_id_);
}

// This function is friended by StackSamplingProfiler so must live directly in
// the base namespace.
void ApplyMetadataToPastSamplesImpl(
    TimeTicks period_start,
    TimeTicks period_end,
    uint64_t name_hash,
    absl::optional<int64_t> key,
    int64_t value,
    absl::optional<PlatformThreadId> thread_id) {
  StackSamplingProfiler::ApplyMetadataToPastSamples(
      period_start, period_end, name_hash, key, value, thread_id);
}

void ApplyMetadataToPastSamples(TimeTicks period_start,
                                TimeTicks period_end,
                                StringPiece name,
                                int64_t value,
                                SampleMetadataScope scope) {
  return ApplyMetadataToPastSamplesImpl(
      period_start, period_end, HashMetricName(name), absl::nullopt, value,
      GetPlatformThreadIdForScope(scope));
}

void ApplyMetadataToPastSamples(TimeTicks period_start,
                                TimeTicks period_end,
                                StringPiece name,
                                int64_t key,
                                int64_t value,
                                SampleMetadataScope scope) {
  return ApplyMetadataToPastSamplesImpl(period_start, period_end,
                                        HashMetricName(name), key, value,
                                        GetPlatformThreadIdForScope(scope));
}

void AddProfileMetadataImpl(uint64_t name_hash,
                            int64_t key,
                            int64_t value,
                            absl::optional<PlatformThreadId> thread_id) {
  StackSamplingProfiler::AddProfileMetadata(name_hash, key, value, thread_id);
}

void AddProfileMetadata(StringPiece name,
                        int64_t key,
                        int64_t value,
                        SampleMetadataScope scope) {
  return AddProfileMetadataImpl(HashMetricName(name), key, value,
                                GetPlatformThreadIdForScope(scope));
}

MetadataRecorder* GetSampleMetadataRecorder() {
  static NoDestructor<MetadataRecorder> instance;
  return instance.get();
}

}  // namespace base
