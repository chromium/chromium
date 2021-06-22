// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/stats_collector.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace base {
namespace internal {

namespace {
void LogStats(size_t swept_bytes, size_t last_size, size_t new_size) {
  VLOG(2) << "quarantine size: " << last_size << " -> " << new_size
          << ", swept bytes: " << swept_bytes
          << ", survival rate: " << static_cast<double>(new_size) / last_size;
}
}  // namespace

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

void StatsCollector::ReportTracesAndHists() const {
  ReportTracesAndHistsImpl<Context::kMutator>(mutator_trace_events_);
  ReportTracesAndHistsImpl<Context::kScanner>(scanner_trace_events_);
  LogStats(swept_size(), quarantine_last_size_, survived_quarantine_size());
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
    const DeferredTraceEventMap<context>& event_map) const {
  std::array<base::TimeDelta, static_cast<size_t>(IdType<context>::kNumIds)>
      accumulated_events{};
  // First, report traces and accumulate each trace scope to report UMA hists.
  for (const auto& tid_and_events : event_map.get_underlying_map_unsafe()) {
    const PlatformThreadId tid = tid_and_events.first;
    // TRACE_EVENT_* macros below drop most parameters when tracing is
    // disabled at compile time.
    ignore_result(tid);
    const auto& events = tid_and_events.second;
    PA_DCHECK(accumulated_events.size() == events.size());
    for (size_t id = 0; id < events.size(); ++id) {
      const auto& event = events[id];
      TRACE_EVENT_BEGIN(kTraceCategory,
                        perfetto::StaticString(
                            ToTracingString(static_cast<IdType<context>>(id))),
                        perfetto::ThreadTrack::ForThread(tid),
                        event.start_time);
      TRACE_EVENT_END(kTraceCategory, perfetto::ThreadTrack::ForThread(tid),
                      event.end_time);
      accumulated_events[id] += (event.end_time - event.start_time);
    }
  }
  // Report UMA if process_name is set.
  if (!process_name_)
    return;
  for (size_t id = 0; id < accumulated_events.size(); ++id) {
    if (accumulated_events[id].is_zero())
      continue;
    UmaHistogramTimes(ToUMAString(static_cast<IdType<context>>(id)).c_str(),
                      accumulated_events[id]);
  }
}

template base::TimeDelta StatsCollector::GetTimeImpl(
    const DeferredTraceEventMap<Context::kMutator>&,
    IdType<Context::kMutator>) const;
template base::TimeDelta StatsCollector::GetTimeImpl(
    const DeferredTraceEventMap<Context::kScanner>&,
    IdType<Context::kScanner>) const;

template void StatsCollector::ReportTracesAndHistsImpl(
    const DeferredTraceEventMap<Context::kMutator>&) const;
template void StatsCollector::ReportTracesAndHistsImpl(
    const DeferredTraceEventMap<Context::kScanner>&) const;

}  // namespace internal
}  // namespace base
