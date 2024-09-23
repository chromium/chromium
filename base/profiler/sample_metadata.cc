// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sample_metadata.h"

#include <optional>
#include <string_view>

#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/threading/thread_local.h"

namespace base {

namespace {

std::optional<PlatformThreadId> GetPlatformThreadIdForScope(
    SampleMetadataScope scope) {
  if (scope == SampleMetadataScope::kProcess)
    return std::nullopt;
  return PlatformThread::CurrentId();
}

}  // namespace

SampleMetadata::SampleMetadata(std::string_view name, SampleMetadataScope scope)
    : name_hash_(HashMetricName(name)), scope_(scope) {}

void SampleMetadata::Set(int64_t value) {
  GetSampleMetadataRecorder()->Set(name_hash_, std::nullopt,
                                   GetPlatformThreadIdForScope(scope_), value);
}

void SampleMetadata::Set(int64_t key, int64_t value) {
  GetSampleMetadataRecorder()->Set(name_hash_, key,
                                   GetPlatformThreadIdForScope(scope_), value);
}

void SampleMetadata::Remove() {
  GetSampleMetadataRecorder()->Remove(name_hash_, std::nullopt,
                                      GetPlatformThreadIdForScope(scope_));
}

void SampleMetadata::Remove(int64_t key) {
  GetSampleMetadataRecorder()->Remove(name_hash_, key,
                                      GetPlatformThreadIdForScope(scope_));
}

ScopedSampleMetadata::ScopedSampleMetadata(std::string_view name,
                                           int64_t value,
                                           SampleMetadataScope scope)
    : name_hash_(HashMetricName(name)),
      thread_id_(GetPlatformThreadIdForScope(scope)) {
  GetSampleMetadataRecorder()->Set(name_hash_, std::nullopt, thread_id_, value);
}

ScopedSampleMetadata::ScopedSampleMetadata(std::string_view name,
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
void ApplyMetadataToPastSamplesImpl(TimeTicks period_start,
                                    TimeTicks period_end,
                                    uint64_t name_hash,
                                    std::optional<int64_t> key,
                                    int64_t value,
                                    std::optional<PlatformThreadId> thread_id) {
  StackSamplingProfiler::ApplyMetadataToPastSamples(
      period_start, period_end, name_hash, key, value, thread_id);
}

void ApplyMetadataToPastSamples(TimeTicks period_start,
                                TimeTicks period_end,
                                std::string_view name,
                                int64_t value,
                                SampleMetadataScope scope) {
  return ApplyMetadataToPastSamplesImpl(
      period_start, period_end, HashMetricName(name), std::nullopt, value,
      GetPlatformThreadIdForScope(scope));
}

void ApplyMetadataToPastSamples(TimeTicks period_start,
                                TimeTicks period_end,
                                std::string_view name,
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
                            std::optional<PlatformThreadId> thread_id) {
  StackSamplingProfiler::AddProfileMetadata(name_hash, key, value, thread_id);
}

void AddProfileMetadata(std::string_view name,
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
