// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/measured_memory_dump_provider_info.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider_info.h"

namespace base::trace_event {

namespace {

// Non-templated wrapper that can be passed to LogUmaHistograms.
void UmaHistogramStatusEnumeration(
    std::string_view name,
    MeasuredMemoryDumpProviderInfo::Status status) {
  base::UmaHistogramEnumeration(name, status);
}

template <typename T>
void LogUmaHistograms(void (*histogram_func)(std::string_view, T),
                      std::string_view base_name,
                      std::string_view provider_name,
                      MemoryDumpLevelOfDetail level_of_detail,
                      T sample) {
  std::string_view level_of_detail_string =
      MeasuredMemoryDumpProviderInfo::LevelOfDetailString(level_of_detail);

  constexpr std::string_view kAllProviders("AllProviders");
  constexpr std::string_view kAllDetailLevels("AllDetailLevels");
  for (std::string_view provider_variant : {provider_name, kAllProviders}) {
    for (std::string_view detail_level_variant :
         {level_of_detail_string, kAllDetailLevels}) {
      histogram_func(
          base::StrCat({"Memory.DumpProvider.", base_name, ".",
                        detail_level_variant, ".", provider_variant}),
          sample);
    }
  }
}

}  // namespace

MeasuredMemoryDumpProviderInfo::MeasuredMemoryDumpProviderInfo() = default;

MeasuredMemoryDumpProviderInfo::MeasuredMemoryDumpProviderInfo(
    scoped_refptr<MemoryDumpProviderInfo> provider_info,
    MemoryDumpRequestArgs request_args)
    : provider_info_(std::move(provider_info)),
      request_args_(std::move(request_args)) {}

MeasuredMemoryDumpProviderInfo::~MeasuredMemoryDumpProviderInfo() {
  if (provider_info_) {
    CHECK(num_following_providers_.has_value());

    const base::TimeDelta total_time = elapsed_timer_.Elapsed();
    const std::optional<base::TimeDelta> post_task_time =
        post_task_timer_ ? std::make_optional(post_task_timer_->Elapsed())
                         : std::nullopt;

    const std::string provider_name = provider_info_->name.histogram_name();
    LogUmaHistograms(&base::UmaHistogramCounts100000, "FollowingProviders",
                     provider_name, request_args_.level_of_detail,
                     static_cast<int>(num_following_providers_.value()));
    LogUmaHistograms(&UmaHistogramStatusEnumeration, "FinalStatus",
                     provider_name, request_args_.level_of_detail, status_);
    LogUmaHistograms(&base::UmaHistogramMediumTimes, "TotalTime", provider_name,
                     request_args_.level_of_detail, total_time);
    if (post_task_time) {
      LogUmaHistograms(&base::UmaHistogramMediumTimes, "PostTaskTime",
                       provider_name, request_args_.level_of_detail,
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

void MeasuredMemoryDumpProviderInfo::LogMemoryDumpTimeHistograms(
    base::TimeDelta time) const {
  LogUmaHistograms(&base::UmaHistogramMicrosecondsTimes, "MemoryDumpTime",
                   provider_info()->name.histogram_name(),
                   request_args_.level_of_detail, time);
}

// static
void MeasuredMemoryDumpProviderInfo::LogProviderCountHistograms(
    std::string_view provider_name,
    MemoryDumpLevelOfDetail level_of_detail,
    size_t count) {
  LogUmaHistograms(&base::UmaHistogramCounts100000, "Count", provider_name,
                   level_of_detail, static_cast<int>(count));
}

// static
std::string_view MeasuredMemoryDumpProviderInfo::LevelOfDetailString(
    MemoryDumpLevelOfDetail level_of_detail) {
  switch (level_of_detail) {
    case MemoryDumpLevelOfDetail::kBackground:
      return "Background";
    case MemoryDumpLevelOfDetail::kLight:
      return "Light";
    case MemoryDumpLevelOfDetail::kDetailed:
      return "Detailed";
  }
  NOTREACHED();
}

}  // namespace base::trace_event
