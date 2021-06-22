// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_COLLECTOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_COLLECTOR_H_

#include <array>
#include <atomic>
#include <mutex>
#include <type_traits>
#include <unordered_map>

#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/starscan_fwd.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {
namespace internal {

#define FOR_ALL_PCSCAN_SCANNER_SCOPES(V) \
  V(Clear)                               \
  V(Scan)                                \
  V(Sweep)                               \
  V(Overall)

#define FOR_ALL_PCSCAN_MUTATOR_SCOPES(V) \
  V(Clear)                               \
  V(ScanStack)                           \
  V(Scan)                                \
  V(Overall)

class StatsCollector final {
 public:
  enum class ScannerId {
#define DECLARE_ENUM(name) k##name,
    FOR_ALL_PCSCAN_SCANNER_SCOPES(DECLARE_ENUM)
#undef DECLARE_ENUM
        kNumIds,
  };

  enum class MutatorId {
#define DECLARE_ENUM(name) k##name,
    FOR_ALL_PCSCAN_MUTATOR_SCOPES(DECLARE_ENUM)
#undef DECLARE_ENUM
        kNumIds,
  };

  template <Context context>
  using IdType =
      std::conditional_t<context == Context::kMutator, MutatorId, ScannerId>;

  // We don't immediately trace events, but instead defer it until scanning is
  // done. This is needed to avoid unpredictable work that can be done by traces
  // (e.g. recursive mutex lock).
  struct DeferredTraceEvent {
    base::TimeTicks start_time;
    base::TimeTicks end_time;
  };

  // Thread-safe hash-map that maps thread id to scanner events. Doesn't
  // accumulate events, i.e. every event can only be registered once.
  template <Context context>
  class DeferredTraceEventMap final {
   public:
    using IdType = StatsCollector::IdType<context>;
    using PerThreadEvents =
        std::array<DeferredTraceEvent, static_cast<size_t>(IdType::kNumIds)>;
    using UnderlyingMap = std::unordered_map<
        PlatformThreadId,
        PerThreadEvents,
        std::hash<PlatformThreadId>,
        std::equal_to<>,
        MetadataAllocator<std::pair<const PlatformThreadId, PerThreadEvents>>>;

    inline void RegisterBeginEventFromCurrentThread(IdType id);
    inline void RegisterEndEventFromCurrentThread(IdType id);

    const UnderlyingMap& get_underlying_map_unsafe() const { return events_; }

   private:
    std::mutex mutex_;
    UnderlyingMap events_;
  };

  template <Context context>
  class Scope final {
   public:
    Scope(StatsCollector& stats, IdType<context> type)
        : stats_(stats), type_(type) {
      stats_.RegisterBeginEventFromCurrentThread(type);
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    ~Scope() { stats_.RegisterEndEventFromCurrentThread(type_); }

   private:
    StatsCollector& stats_;
    IdType<context> type_;
  };

  using ScannerScope = Scope<Context::kScanner>;
  using MutatorScope = Scope<Context::kMutator>;

  StatsCollector(const char* process_name, size_t quarantine_last_size);

  StatsCollector(const StatsCollector&) = delete;
  StatsCollector& operator=(const StatsCollector&) = delete;

  ~StatsCollector();

  void IncreaseSurvivedQuarantineSize(size_t size) {
    survived_quarantine_size_.fetch_add(size, std::memory_order_relaxed);
  }
  size_t survived_quarantine_size() const {
    return survived_quarantine_size_.load(std::memory_order_relaxed);
  }

  void IncreaseSweptSize(size_t size) { swept_size_ += size; }
  size_t swept_size() const { return swept_size_; }

  base::TimeDelta GetOverallTime() const;
  void ReportTracesAndHists() const;

 private:
  using MetadataString =
      std::basic_string<char, std::char_traits<char>, MetadataAllocator<char>>;
  static constexpr char kTraceCategory[] = "partition_alloc";

  static constexpr const char* ToTracingString(ScannerId id);
  static constexpr const char* ToTracingString(MutatorId id);

  MetadataString ToUMAString(ScannerId id) const;
  MetadataString ToUMAString(MutatorId id) const;

  void RegisterBeginEventFromCurrentThread(MutatorId id) {
    mutator_trace_events_.RegisterBeginEventFromCurrentThread(id);
  }
  void RegisterEndEventFromCurrentThread(MutatorId id) {
    mutator_trace_events_.RegisterEndEventFromCurrentThread(id);
  }
  void RegisterBeginEventFromCurrentThread(ScannerId id) {
    scanner_trace_events_.RegisterBeginEventFromCurrentThread(id);
  }
  void RegisterEndEventFromCurrentThread(ScannerId id) {
    scanner_trace_events_.RegisterEndEventFromCurrentThread(id);
  }

  template <Context context>
  base::TimeDelta GetTimeImpl(const DeferredTraceEventMap<context>& event_map,
                              IdType<context> id) const;

  template <Context context>
  void ReportTracesAndHistsImpl(
      const DeferredTraceEventMap<context>& event_map) const;

  void ReportSurvivalRate() const;

  DeferredTraceEventMap<Context::kMutator> mutator_trace_events_;
  DeferredTraceEventMap<Context::kScanner> scanner_trace_events_;

  std::atomic<size_t> survived_quarantine_size_{0u};
  size_t swept_size_ = 0u;
  const char* process_name_ = nullptr;
  const size_t quarantine_last_size_ = 0u;
};

template <Context context>
inline void StatsCollector::DeferredTraceEventMap<
    context>::RegisterBeginEventFromCurrentThread(IdType id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto tid = base::PlatformThread::CurrentId();
  const auto now = base::TimeTicks::Now();
  auto& event_array = events_[tid];
  auto& event = event_array[static_cast<size_t>(id)];
  PA_DCHECK(event.start_time.is_null());
  PA_DCHECK(event.end_time.is_null());
  event.start_time = now;
}

template <Context context>
inline void StatsCollector::DeferredTraceEventMap<
    context>::RegisterEndEventFromCurrentThread(IdType id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto tid = base::PlatformThread::CurrentId();
  const auto now = base::TimeTicks::Now();
  auto& event_array = events_[tid];
  auto& event = event_array[static_cast<size_t>(id)];
  PA_DCHECK(!event.start_time.is_null());
  PA_DCHECK(event.end_time.is_null());
  event.end_time = now;
}

inline constexpr const char* StatsCollector::ToTracingString(ScannerId id) {
  switch (id) {
    case ScannerId::kClear:
      return "PCScan.Scanner.Clear";
    case ScannerId::kScan:
      return "PCScan.Scanner.Scan";
    case ScannerId::kSweep:
      return "PCScan.Scanner.Sweep";
    case ScannerId::kOverall:
      return "PCScan.Scanner";
    case ScannerId::kNumIds:
      __builtin_unreachable();
  }
}

inline constexpr const char* StatsCollector::ToTracingString(MutatorId id) {
  switch (id) {
    case MutatorId::kClear:
      return "PCScan.Mutator.Clear";
    case MutatorId::kScanStack:
      return "PCScan.Mutator.ScanStack";
    case MutatorId::kScan:
      return "PCScan.Mutator.Scan";
    case MutatorId::kOverall:
      return "PCScan.Mutator";
    case MutatorId::kNumIds:
      __builtin_unreachable();
  }
}

inline StatsCollector::MetadataString StatsCollector::ToUMAString(
    ScannerId id) const {
  PA_DCHECK(process_name_);
  const MetadataString process_name = process_name_;
  switch (id) {
    case ScannerId::kClear:
      return "PA.PCScan." + process_name + ".Scanner.Clear";
    case ScannerId::kScan:
      return "PA.PCScan." + process_name + ".Scanner.Scan";
    case ScannerId::kSweep:
      return "PA.PCScan." + process_name + ".Scanner.Sweep";
    case ScannerId::kOverall:
      return "PA.PCScan." + process_name + ".Scanner";
    case ScannerId::kNumIds:
      __builtin_unreachable();
  }
}

inline StatsCollector::MetadataString StatsCollector::ToUMAString(
    MutatorId id) const {
  PA_DCHECK(process_name_);
  const MetadataString process_name = process_name_;
  switch (id) {
    case MutatorId::kClear:
      return "PA.PCScan." + process_name + ".Mutator.Clear";
    case MutatorId::kScanStack:
      return "PA.PCScan." + process_name + ".Mutator.ScanStack";
    case MutatorId::kScan:
      return "PA.PCScan." + process_name + ".Mutator.Scan";
    case MutatorId::kOverall:
      return "PA.PCScan." + process_name + ".Mutator";
    case MutatorId::kNumIds:
      __builtin_unreachable();
  }
}

#undef FOR_ALL_PCSCAN_MUTATOR_SCOPES
#undef FOR_ALL_PCSCAN_SCANNER_SCOPES

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATS_COLLECTOR_H_
