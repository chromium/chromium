// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/measured_memory_dump_provider_info.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
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
    const std::optional<base::TimeDelta> post_task_time =
        post_task_timer_ ? std::make_optional(post_task_timer_->Elapsed())
                         : std::nullopt;

    base::UmaHistogramCounts100000(
        base::StrCat({"Memory.DumpProvider.FollowingProviders3.",
                      provider_info_->name.histogram_name()}),
        static_cast<int>(num_following_providers_));
    base::UmaHistogramEnumeration(
        base::StrCat({"Memory.DumpProvider.FinalStatus.",
                      provider_info_->name.histogram_name()}),
        status_);
    base::UmaHistogramMediumTimes(
        base::StrCat({"Memory.DumpProvider.TotalTime2.",
                      provider_info_->name.histogram_name()}),
        total_time);
    if (post_task_time) {
      base::UmaHistogramMediumTimes(
          base::StrCat({"Memory.DumpProvider.PostTaskTime.",
                        provider_info_->name.histogram_name()}),
          *post_task_time);
    }

    // Aggregate all providers together without a suffix.
    base::UmaHistogramCounts100000("Memory.DumpProvider.FollowingProviders3",
                                   static_cast<int>(num_following_providers_));
    base::UmaHistogramEnumeration("Memory.DumpProvider.FinalStatus", status_);
    base::UmaHistogramMediumTimes("Memory.DumpProvider.TotalTime2", total_time);
    if (post_task_time) {
      base::UmaHistogramMediumTimes("Memory.DumpProvider.PostTaskTime",
                                    *post_task_time);
    }
  }
}

MeasuredMemoryDumpProviderInfo::MeasuredMemoryDumpProviderInfo(
    MeasuredMemoryDumpProviderInfo&&) = default;

MeasuredMemoryDumpProviderInfo& MeasuredMemoryDumpProviderInfo::operator=(
    MeasuredMemoryDumpProviderInfo&&) = default;

void MeasuredMemoryDumpProviderInfo::SetStatus(Status status) {
  CHECK_NE(status, status_);
  if (status == Status::kPosted) {
    // Start `post_task_timer_`.
    CHECK(!post_task_timer_.has_value());
    post_task_timer_.emplace();
  }
  status_ = status;
}

}  // namespace base::trace_event
