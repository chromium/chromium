// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/stats_collector.h"

#include "base/allocator/partition_allocator/partition_alloc_base/time/time.h"
#include "base/allocator/partition_allocator/starscan/logging.h"
#include "base/allocator/partition_allocator/starscan/stats_reporter.h"

namespace partition_alloc::internal {

StatsCollector::StatsCollector(const char* process_name,
                               size_t quarantine_last_size)
    : process_name_(process_name),
      quarantine_last_size_(quarantine_last_size) {}

StatsCollector::~StatsCollector() = default;

base::TimeDelta StatsCollector::GetOverallTime() const {
  return GetTimeImpl<Context::kMutator>(mutator_trace_events_,
                                        MutatorId::kOverall) +
         GetTimeImpl<Context::kScanner>(scanner_trace_events_,
                                        ScannerId::kOverall);
}

void StatsCollector::ReportTracesAndHists(
    partition_alloc::StatsReporter& reporter) const {
  ReportTracesAndHistsImpl<Context::kMutator>(reporter, mutator_trace_events_);
  ReportTracesAndHistsImpl<Context::kScanner>(reporter, scanner_trace_events_);
  ReportSurvivalRate(reporter);
}

template <Context context>
base::TimeDelta StatsCollector::GetTimeImpl(
    const DeferredTraceEventMap<context>& event_map,
    IdType<context> id) const {
  base::TimeDelta overall;
  for (const auto& tid_and_events : event_map.get_underlying_map_unsafe()) {
    const auto& events = tid_and_events.second;
    const auto& event = events[static_cast<size_t>(id)];
    overall += (event.end_time - event.start_time);
  }
  return overall;
}

template <Context context>
void StatsCollector::ReportTracesAndHistsImpl(
    partition_alloc::StatsReporter& reporter,
    const DeferredTraceEventMap<context>& event_map) const {
  std::array<base::TimeDelta, static_cast<size_t>(IdType<context>::kNumIds)>
      accumulated_events{};
  // First, report traces and accumulate each trace scope to report UMA hists.
  for (const auto& tid_and_events : event_map.get_underlying_map_unsafe()) {
    const internal::base::PlatformThreadId tid = tid_and_events.first;
    const auto& events = tid_and_events.second;
    PA_DCHECK(accumulated_events.size() == events.size());
    for (size_t id = 0; id < events.size(); ++id) {
      const auto& event = events[id];
      if (event.start_time.is_null()) {
        // If start_time is null, the event was never triggered, e.g. safepoint
        // bailed out if started at the end of scanning.
        PA_DCHECK(event.end_time.is_null());
        continue;
      }
      reporter.ReportTraceEvent(static_cast<IdType<context>>(id), tid,
                                event.start_time.ToInternalValue(),
                                event.end_time.ToInternalValue());
      accumulated_events[id] += (event.end_time - event.start_time);
    }
  }
  // Report UMA if process_name is set.
  if (!process_name_) {
    return;
  }
  for (size_t id = 0; id < accumulated_events.size(); ++id) {
    if (accumulated_events[id].is_zero()) {
      continue;
    }
    reporter.ReportStats(ToUMAString(static_cast<IdType<context>>(id)).c_str(),
                         accumulated_events[id].InMicroseconds());
  }
}

void StatsCollector::ReportSurvivalRate(
    partition_alloc::StatsReporter& reporter) const {
  const double survived_rate =
      static_cast<double>(survived_quarantine_size()) / quarantine_last_size_;
  reporter.ReportSurvivedQuarantineSize(survived_quarantine_size());
  reporter.ReportSurvivedQuarantinePercent(survived_rate);
  PA_PCSCAN_VLOG(2) << "quarantine size: " << quarantine_last_size_ << " -> "
                    << survived_quarantine_size()
                    << ", swept bytes: " << swept_size()
                    << ", survival rate: " << survived_rate;
  if (discarded_quarantine_size_) {
    PA_PCSCAN_VLOG(2) << "discarded quarantine size: "
                      << discarded_quarantine_size_;
  }
}

template base::TimeDelta StatsCollector::GetTimeImpl(
    const DeferredTraceEventMap<Context::kMutator>&,
    IdType<Context::kMutator>) const;
template base::TimeDelta StatsCollector::GetTimeImpl(
    const DeferredTraceEventMap<Context::kScanner>&,
    IdType<Context::kScanner>) const;

template void StatsCollector::ReportTracesAndHistsImpl(
    partition_alloc::StatsReporter& reporter,
    const DeferredTraceEventMap<Context::kMutator>&) const;
template void StatsCollector::ReportTracesAndHistsImpl(
    partition_alloc::StatsReporter& reporter,
    const DeferredTraceEventMap<Context::kScanner>&) const;

}  // namespace partition_alloc::internal
