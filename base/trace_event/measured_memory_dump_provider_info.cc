// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/measured_memory_dump_provider_info.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider_info.h"

namespace base::trace_event {

MeasuredMemoryDumpProviderInfo::MeasuredMemoryDumpProviderInfo()
    : MeasuredMemoryDumpProviderInfo(nullptr, 0u) {}

MeasuredMemoryDumpProviderInfo::MeasuredMemoryDumpProviderInfo(
    scoped_refptr<MemoryDumpProviderInfo> provider_info,
    size_t num_following_providers)
    : provider_info_(std::move(provider_info)),
      num_following_providers_(num_following_providers) {}

MeasuredMemoryDumpProviderInfo::~MeasuredMemoryDumpProviderInfo() {
  if (provider_info_) {
    const base::TimeDelta total_time = elapsed_timer_.Elapsed();

    base::UmaHistogramCounts1000(
        base::StrCat({"Memory.DumpProvider.FollowingProviders2.",
                      provider_info_->name.histogram_name()}),
        static_cast<int>(num_following_providers_));
    base::UmaHistogramEnumeration(
        base::StrCat({"Memory.DumpProvider.FinalStatus.",
                      provider_info_->name.histogram_name()}),
        status_);
    base::UmaHistogramMicrosecondsTimes(
        base::StrCat({"Memory.DumpProvider.TotalTime.",
                      provider_info_->name.histogram_name()}),
        total_time);

    // Aggregate all providers together without a suffix.
    base::UmaHistogramCounts1000("Memory.DumpProvider.FollowingProviders2",
                                 static_cast<int>(num_following_providers_));
    base::UmaHistogramEnumeration("Memory.DumpProvider.FinalStatus", status_);
    base::UmaHistogramMicrosecondsTimes("Memory.DumpProvider.TotalTime",
                                        total_time);
  }
}

MeasuredMemoryDumpProviderInfo::MeasuredMemoryDumpProviderInfo(
    MeasuredMemoryDumpProviderInfo&&) = default;

MeasuredMemoryDumpProviderInfo& MeasuredMemoryDumpProviderInfo::operator=(
    MeasuredMemoryDumpProviderInfo&&) = default;

}  // namespace base::trace_event
