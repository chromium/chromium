// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/allocator/partition_allocator/starscan/raceful_worklist.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/compiler_specific.h"
#include "base/cpu.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_64)
// Include order is important, so we disable formatting.
// clang-format off
// Including these headers directly should generally be avoided. For the
// scanning loop, we check at runtime which SIMD extension we can use. Since
// Chrome is compiled with -msse3 (the minimal requirement), we include the
// headers directly to make the intrinsics available. Another option could be to
// use inline assembly, but that would hinder compiler optimization for
// vectorized instructions.
#include <immintrin.h>
#include <smmintrin.h>
#include <avxintrin.h>
#include <avx2intrin.h>
// clang-format on
#endif

namespace base {
namespace internal {

[[noreturn]] BASE_EXPORT NOINLINE NOT_TAIL_CALLED void DoubleFreeAttempt() {
  NO_CODE_FOLDING();
  IMMEDIATE_CRASH();
}

namespace {

#if DCHECK_IS_ON() && defined(OS_LINUX)
// Currently, check reentracy only on Linux. On Android TLS is emulated by the
// runtime lib, which can allocate and therefore cause reentrancy.
struct ReentrantScannerGuard final {
 public:
  ReentrantScannerGuard() {
    PA_CHECK(!guard_);
    guard_ = true;
  }
  ~ReentrantScannerGuard() { guard_ = false; }

 private:
  // Since this variable has hidden visibility (not referenced by other DSOs),
  // assume that thread_local works on all supported architectures.
  static thread_local size_t guard_;
};
thread_local size_t ReentrantScannerGuard::guard_ = 0;
#else
struct [[maybe_unused]] ReentrantScannerGuard final{};
#endif

#if defined(PA_HAS_64_BITS_POINTERS)
// Bytemap that represent regions (cards) that contain quarantined objects.
// A single PCScan cycle consists of the following steps:
// 1) clearing (memset quarantine + marking cards that contain quarantine);
// 2) scanning;
// 3) sweeping (freeing + unmarking cards that contain freed objects).
// Marking cards on step 1) ensures that the card table stays in the consistent
// state while scanning. Unmarking on the step 3) ensures that unmarking
// actually happens (and we don't hit too many false positives).
class QuarantineCardTable final {
 public:
  // Avoid the load of the base of the BRP pool.
  ALWAYS_INLINE static QuarantineCardTable& GetFrom(uintptr_t ptr) {
    constexpr uintptr_t kBRPPoolMask = PartitionAddressSpace::BRPPoolBaseMask();
    return *reinterpret_cast<QuarantineCardTable*>(ptr & kBRPPoolMask);
  }

  ALWAYS_INLINE void Quarantine(uintptr_t begin, size_t size) {
    return SetImpl(begin, size, true);
  }

  ALWAYS_INLINE void Unquarantine(uintptr_t begin, size_t size) {
    return SetImpl(begin, size, false);
  }

  // Returns whether the card to which |ptr| points to contains quarantined
  // objects. May return false positives for but should never return false
  // negatives, as otherwise this breaks security.
  ALWAYS_INLINE bool IsQuarantined(uintptr_t ptr) const {
    const size_t byte = Byte(ptr);
    PA_DCHECK(byte < bytes_.size());
    return bytes_[byte];
  }

 private:
  static constexpr size_t kCardSize =
      AddressPoolManager::kBRPPoolMaxSize / kSuperPageSize;
  static constexpr size_t kBytes =
      AddressPoolManager::kBRPPoolMaxSize / kCardSize;

  QuarantineCardTable() = default;

  ALWAYS_INLINE static constexpr size_t Byte(uintptr_t address) {
    constexpr uintptr_t kBRPPoolMask = PartitionAddressSpace::BRPPoolBaseMask();
    return (address & ~kBRPPoolMask) / kCardSize;
  }

  ALWAYS_INLINE void SetImpl(uintptr_t begin, size_t size, bool value) {
    const size_t byte = Byte(begin);
    const size_t need_bytes = (size + (kCardSize - 1)) / kCardSize;
    PA_DCHECK(bytes_.size() >= byte + need_bytes);
    PA_DCHECK(
        PartitionAddressSpace::IsInBRPPool(reinterpret_cast<void*>(begin)));
    for (size_t i = byte; i < byte + need_bytes; ++i)
      bytes_[i] = value;
  }

  std::array<bool, kBytes> bytes_;
};
static_assert(kSuperPageSize >= sizeof(QuarantineCardTable),
              "Card table size must be less than kSuperPageSize, since this is "
              "what is committed");
#endif

template <typename T>
using MetadataVector = std::vector<T, MetadataAllocator<T>>;
template <typename T>
using MetadataSet = std::set<T, std::less<>, MetadataAllocator<T>>;
template <typename K, typename V>
using MetadataHashMap =
    std::unordered_map<K,
                       V,
                       std::hash<K>,
                       std::equal_to<>,
                       MetadataAllocator<std::pair<const K, V>>>;

void LogStats(size_t swept_bytes, size_t last_size, size_t new_size) {
  VLOG(2) << "quarantine size: " << last_size << " -> " << new_size
          << ", swept bytes: " << swept_bytes
          << ", survival rate: " << static_cast<double>(new_size) / last_size;
}

ALWAYS_INLINE uintptr_t GetObjectStartInSuperPage(uintptr_t maybe_ptr,
                                                  const PCScan::Root& root) {
  char* allocation_start =
      GetSlotStartInSuperPage<ThreadSafe>(reinterpret_cast<char*>(maybe_ptr));
  if (!allocation_start) {
    // |maybe_ptr| refers to a garbage or is outside of the payload region.
    return 0;
  }
  return reinterpret_cast<uintptr_t>(
      root.AdjustPointerForExtrasAdd(allocation_start));
}

enum class Context {
  // For tasks executed from mutator threads (safepoints).
  kMutator,
  // For concurrent scanner tasks.
  kScanner
};

#if DCHECK_IS_ON()
bool IsScannerQuarantineBitmapEmpty(char* super_page, size_t epoch) {
  auto* bitmap = QuarantineBitmapFromPointer(QuarantineBitmapType::kScanner,
                                             epoch, super_page);
  size_t visited = 0;
  bitmap->Iterate([&visited](auto) { ++visited; });
  return !visited;
}
#endif

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
    using UnderlyingMap = MetadataHashMap<
        PlatformThreadId,
        std::array<DeferredTraceEvent, static_cast<size_t>(IdType::kNumIds)>>;

    void RegisterBeginEventFromCurrentThread(IdType id) {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto tid = base::PlatformThread::CurrentId();
      const auto now = base::TimeTicks::Now();
      auto& event_array = events_[tid];
      auto& event = event_array[static_cast<size_t>(id)];
      PA_DCHECK(event.start_time.is_null());
      PA_DCHECK(event.end_time.is_null());
      event.start_time = now;
    }

    void RegisterEndEventFromCurrentThread(IdType id) {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto tid = base::PlatformThread::CurrentId();
      const auto now = base::TimeTicks::Now();
      auto& event_array = events_[tid];
      auto& event = event_array[static_cast<size_t>(id)];
      PA_DCHECK(!event.start_time.is_null());
      PA_DCHECK(event.end_time.is_null());
      event.end_time = now;
    }

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

  explicit StatsCollector(const char* process_name)
      : process_name_(process_name) {}

  StatsCollector(const StatsCollector&) = delete;
  StatsCollector& operator=(const StatsCollector&) = delete;

  void IncreaseSurvivedQuarantineSize(size_t size) {
    survived_quarantine_size_.fetch_add(size, std::memory_order_relaxed);
  }
  size_t survived_quarantine_size() const {
    return survived_quarantine_size_.load(std::memory_order_relaxed);
  }

  void IncreaseSweptSize(size_t size) { swept_size_ += size; }
  size_t swept_size() const { return swept_size_; }

  base::TimeDelta GetOverallTime() const {
    return GetTimeImpl<Context::kMutator>(mutator_trace_events_,
                                          MutatorId::kOverall) +
           GetTimeImpl<Context::kScanner>(scanner_trace_events_,
                                          ScannerId::kOverall);
  }

  void ReportTracesAndHists() {
    ReportTracesAndHistsImpl<Context::kMutator>(mutator_trace_events_);
    ReportTracesAndHistsImpl<Context::kScanner>(scanner_trace_events_);
  }

 private:
  using MetadataString =
      std::basic_string<char, std::char_traits<char>, MetadataAllocator<char>>;
  static constexpr char kTraceCategory[] = "partition_alloc";

  static constexpr const char* ToTracingString(ScannerId id) {
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

  static constexpr const char* ToTracingString(MutatorId id) {
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

  MetadataString ToUMAString(ScannerId id) const {
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

  MetadataString ToUMAString(MutatorId id) const {
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

  template <Context context, typename EventMap>
  base::TimeDelta GetTimeImpl(const EventMap& event_map,
                              IdType<context> id) const {
    base::TimeDelta overall;
    for (const auto& tid_and_events : event_map.get_underlying_map_unsafe()) {
      const auto& events = tid_and_events.second;
      const auto& event = events[static_cast<size_t>(id)];
      overall += (event.end_time - event.start_time);
    }
    return overall;
  }

  template <Context context, typename EventMap>
  void ReportTracesAndHistsImpl(const EventMap& event_map) {
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
        TRACE_EVENT_BEGIN(
            kTraceCategory,
            perfetto::StaticString(
                ToTracingString(static_cast<IdType<context>>(id))),
            perfetto::ThreadTrack::ForThread(tid), event.start_time);
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

  DeferredTraceEventMap<Context::kMutator> mutator_trace_events_;
  DeferredTraceEventMap<Context::kScanner> scanner_trace_events_;

  std::atomic<size_t> survived_quarantine_size_{0u};
  size_t swept_size_ = 0u;
  const char* process_name_ = nullptr;
};

#undef FOR_ALL_PCSCAN_MUTATOR_SCOPES
#undef FOR_ALL_PCSCAN_SCANNER_SCOPES

enum class SimdSupport : uint8_t {
  kUnvectorized,
  kSSE41,
  kAVX2,
  // TODO(bikineev): Add support for Neon.
};

SimdSupport DetectSimdSupport() {
  base::CPU cpu;
  if (cpu.has_avx2())
    return SimdSupport::kAVX2;
  if (cpu.has_sse41())
    return SimdSupport::kSSE41;
  return SimdSupport::kUnvectorized;
}

// Internal PCScan singleton. The separation between frontend and backend is
// needed to keep access to the hot data (quarantine) in the frontend fast,
// whereas the backend can hold cold data.
class PCScanInternal final {
 public:
  using Root = PCScan::Root;
  using TaskHandle = scoped_refptr<PCScanTask>;

  static constexpr size_t kMaxNumberOfRoots = 8u;
  class Roots final : private std::array<Root*, kMaxNumberOfRoots> {
    using Base = std::array<Root*, kMaxNumberOfRoots>;

   public:
    using typename Base::const_iterator;
    using typename Base::iterator;

    // Explicitly value-initialize Base{} as otherwise the default
    // (aggregate) initialization won't be considered as constexpr.
    constexpr Roots() : Base{} {}

    iterator begin() { return Base::begin(); }
    const_iterator begin() const { return Base::begin(); }

    iterator end() { return begin() + current_; }
    const_iterator end() const { return begin() + current_; }

    void Add(Root* root);

    size_t size() const { return current_; }

    void ClearForTesting();  // IN-TEST

   private:
    size_t current_ = 0u;
  };

  static PCScanInternal& Instance() {
    // Since the data that PCScanInternal holds is cold, it's fine to have the
    // runtime check for thread-safe local static initialization.
    static base::NoDestructor<PCScanInternal> instance;
    return *instance;
  }

  PCScanInternal(const PCScanInternal&) = delete;
  PCScanInternal& operator=(const PCScanInternal&) = delete;

  TaskHandle CurrentPCScanTask() const {
    std::lock_guard<std::mutex> lock(current_task_mutex_);
    return current_task_;
  }
  void SetCurrentPCScanTask(TaskHandle task) {
    std::lock_guard<std::mutex> lock(current_task_mutex_);
    current_task_ = std::move(task);
  }
  void ResetCurrentPCScanTask() {
    std::lock_guard<std::mutex> lock(current_task_mutex_);
    current_task_.reset();
  }

  void RegisterScannableRoot(Root* root);
  void RegisterNonScannableRoot(Root* root);

  Roots& scannable_roots() { return scannable_roots_; }
  const Roots& scannable_roots() const { return scannable_roots_; }

  Roots& nonscannable_roots() { return nonscannable_roots_; }
  const Roots& nonscannable_roots() const { return nonscannable_roots_; }

  void SetProcessName(const char* name);
  const char* process_name() const { return process_name_; }

  // Get size of all committed pages from scannable and nonscannable roots.
  size_t CalculateTotalHeapSize() const;

  SimdSupport simd_support() const { return simd_support_; }

  void EnableStackScanning();
  void DisableStackScanning();
  bool IsStackScanningEnabled() const;

  void NotifyThreadCreated(void* stack_top);
  void NotifyThreadDestroyed();

  void* GetCurrentThreadStackTop() const;

  void ClearRootsForTesting();  // IN-TEST
  void ReinitForTesting();      // IN-TEST

 private:
  using StackTops = MetadataHashMap<PlatformThreadId, void*>;

  friend base::NoDestructor<PCScanInternal>;

  PCScanInternal();

  TaskHandle current_task_;
  mutable std::mutex current_task_mutex_;

  Roots scannable_roots_{};
  Roots nonscannable_roots_{};

  bool stack_scanning_enabled_{false};
  // TLS emulation of stack tops. Since this is guaranteed to go through
  // non-quarantinable partition, using it from safepoints is safe.
  StackTops stack_tops_;
  mutable std::mutex stack_tops_mutex_;

  const char* process_name_ = nullptr;
  const SimdSupport simd_support_;
};

void PCScanInternal::Roots::Add(Root* root) {
  PA_CHECK(std::find(begin(), end(), root) == end());
  (*this)[current_] = root;
  ++current_;
  PA_CHECK(current_ != kMaxNumberOfRoots)
      << "Exceeded number of allowed partition roots";
}

void PCScanInternal::Roots::ClearForTesting() {
  std::fill(begin(), end(), nullptr);
  current_ = 0;
}

void CommitCardTable() {
#if defined(PA_HAS_64_BITS_POINTERS)
  if (features::IsPartitionAllocGigaCageEnabled()) {
    // First, make sure that GigaCage is initialized.
    PartitionAddressSpace::Init();
    // Then, commit the card table.
    RecommitSystemPages(
        reinterpret_cast<void*>(PartitionAddressSpace::BRPPoolBase()),
        sizeof(QuarantineCardTable), PageReadWrite, PageUpdatePermissions);
  }
#endif
}

void CommitQuarantineBitmaps(PCScan::Root& root) {
  size_t quarantine_bitmaps_size_to_commit = CommittedQuarantineBitmapsSize();
  for (auto* super_page_extent = root.first_extent; super_page_extent;
       super_page_extent = super_page_extent->next) {
    for (char* super_page = super_page_extent->super_page_base;
         super_page != super_page_extent->super_pages_end;
         super_page += kSuperPageSize) {
      RecommitSystemPages(internal::SuperPageQuarantineBitmaps(super_page),
                          quarantine_bitmaps_size_to_commit, PageReadWrite,
                          PageUpdatePermissions);
    }
  }
}

PCScanInternal::PCScanInternal() : simd_support_(DetectSimdSupport()) {
  CommitCardTable();
}

void PCScanInternal::RegisterScannableRoot(Root* root) {
  PA_DCHECK(root);
  PA_CHECK(root->IsQuarantineAllowed());
  typename Root::ScopedGuard guard(root->lock_);
  if (root->IsScanEnabled())
    return;
  PA_CHECK(!root->IsQuarantineEnabled());
  CommitQuarantineBitmaps(*root);
  root->scan_mode = Root::ScanMode::kEnabled;
  root->quarantine_mode = Root::QuarantineMode::kEnabled;
  scannable_roots_.Add(root);
}

void PCScanInternal::RegisterNonScannableRoot(Root* root) {
  PA_DCHECK(root);
  PA_CHECK(root->IsQuarantineAllowed());
  typename Root::ScopedGuard guard(root->lock_);
  if (root->IsQuarantineEnabled())
    return;
  CommitQuarantineBitmaps(*root);
  root->quarantine_mode = Root::QuarantineMode::kEnabled;
  nonscannable_roots_.Add(root);
}

void PCScanInternal::SetProcessName(const char* process_name) {
  PA_DCHECK(process_name);
  PA_DCHECK(!process_name_);
  process_name_ = process_name;
}

size_t PCScanInternal::CalculateTotalHeapSize() const {
  const auto acc = [](size_t size, Root* root) {
    return size + root->get_total_size_of_committed_pages();
  };
  return std::accumulate(scannable_roots_.begin(), scannable_roots_.end(), 0u,
                         acc) +
         std::accumulate(nonscannable_roots_.begin(), nonscannable_roots_.end(),
                         0u, acc);
}

void PCScanInternal::EnableStackScanning() {
  PA_DCHECK(!stack_scanning_enabled_);
  stack_scanning_enabled_ = true;
}
void PCScanInternal::DisableStackScanning() {
  PA_DCHECK(stack_scanning_enabled_);
  stack_scanning_enabled_ = false;
}
bool PCScanInternal::IsStackScanningEnabled() const {
  return stack_scanning_enabled_;
}

void PCScanInternal::NotifyThreadCreated(void* stack_top) {
  const auto tid = base::PlatformThread::CurrentId();
  std::lock_guard<std::mutex> lock(stack_tops_mutex_);
  const auto res = stack_tops_.insert({tid, stack_top});
  PA_DCHECK(res.second);
}

void PCScanInternal::NotifyThreadDestroyed() {
  const auto tid = base::PlatformThread::CurrentId();
  std::lock_guard<std::mutex> lock(stack_tops_mutex_);
  PA_DCHECK(1 == stack_tops_.count(tid));
  stack_tops_.erase(tid);
}

void* PCScanInternal::GetCurrentThreadStackTop() const {
  const auto tid = base::PlatformThread::CurrentId();
  std::lock_guard<std::mutex> lock(stack_tops_mutex_);
  auto it = stack_tops_.find(tid);
  return it != stack_tops_.end() ? it->second : nullptr;
}

void PCScanInternal::ClearRootsForTesting() {
  // Set all roots as non-scannable and non-quarantinable.
  for (auto* root : scannable_roots_) {
    root->scan_mode = Root::ScanMode::kDisabled;
    root->quarantine_mode = Root::QuarantineMode::kDisabledByDefault;
  }
  for (auto* root : nonscannable_roots_) {
    root->quarantine_mode = Root::QuarantineMode::kDisabledByDefault;
  }
  scannable_roots_.ClearForTesting();     // IN-TEST
  nonscannable_roots_.ClearForTesting();  // IN-TEST
}

void PCScanInternal::ReinitForTesting() {
  CommitCardTable();
}

class PCScanSnapshot final {
 public:
  struct ScanArea {
    ScanArea(uintptr_t* begin, uintptr_t* end) : begin(begin), end(end) {}

    uintptr_t* begin = nullptr;
    uintptr_t* end = nullptr;
  };

  // Large scan areas have their slot size recorded which allows to iterate
  // based on objects, potentially skipping over objects if possible.
  struct LargeScanArea : public ScanArea {
    LargeScanArea(uintptr_t* begin, uintptr_t* end, size_t slot_size)
        : ScanArea(begin, end), slot_size(slot_size) {}

    size_t slot_size = 0;
  };

  // Worklists that are shared and processed by different scanners.
  using ScanAreasWorklist = RacefulWorklist<ScanArea>;
  using LargeScanAreasWorklist = RacefulWorklist<LargeScanArea>;
  using SuperPagesWorklist = RacefulWorklist<uintptr_t>;

  // BRP pool is guaranteed to have only normal buckets, so everything there
  // deals in super pages.
  using SuperPages = MetadataSet<uintptr_t>;

  PCScanSnapshot() = default;

  void EnsureTaken(size_t pcscan_epoch);

  ScanAreasWorklist& scan_areas_worklist() { return scan_areas_worklist_; }
  LargeScanAreasWorklist& large_scan_areas_worklist() {
    return large_scan_areas_worklist_;
  }
  SuperPagesWorklist& quarantinable_super_pages_worklist() {
    return super_pages_worklist_;
  }

  const SuperPages& quarantinable_super_pages() const { return super_pages_; }

 private:
  void Take(size_t pcscan_epoch);

  SuperPages super_pages_;

  ScanAreasWorklist scan_areas_worklist_;
  LargeScanAreasWorklist large_scan_areas_worklist_;
  SuperPagesWorklist super_pages_worklist_;

  std::once_flag once_flag_;
};

void PCScanSnapshot::EnsureTaken(size_t pcscan_epoch) {
  std::call_once(once_flag_, &PCScanSnapshot::Take, this, pcscan_epoch);
}

void PCScanSnapshot::Take(size_t pcscan_epoch) {
  using Root = PartitionRoot<ThreadSafe>;
  using SlotSpan = SlotSpanMetadata<ThreadSafe>;
  // Threshold for which bucket size it is worthwhile in checking whether the
  // object is a quarantined object and can be skipped.
  static constexpr size_t kLargeScanAreaThreshold = 8192;

  auto& pcscan_internal = PCScanInternal::Instance();
  for (Root* root : pcscan_internal.scannable_roots()) {
    typename Root::ScopedGuard guard(root->lock_);

    // Take a snapshot of all super pages and scannable slot spans.
    // TODO(bikineev): Consider making current_extent lock-free and moving it
    // to the concurrent thread.
    for (auto* super_page_extent = root->first_extent; super_page_extent;
         super_page_extent = super_page_extent->next) {
      for (char* super_page = super_page_extent->super_page_base;
           super_page != super_page_extent->super_pages_end;
           super_page += kSuperPageSize) {
        // TODO(bikineev): Consider following freelists instead of slot spans.
        const size_t visited_slot_spans = IterateSlotSpans<ThreadSafe>(
            super_page, true /*with_quarantine*/,
            [this](SlotSpan* slot_span) -> bool {
              if (slot_span->is_empty() || slot_span->is_decommitted()) {
                return false;
              }
              auto* payload_begin = static_cast<uintptr_t*>(
                  SlotSpan::ToSlotSpanStartPtr(slot_span));
              size_t provisioned_size = slot_span->GetProvisionedSize();
              // Free & decommitted slot spans are skipped.
              PA_DCHECK(provisioned_size > 0);
              auto* payload_end =
                  payload_begin + (provisioned_size / sizeof(uintptr_t));
              if (slot_span->bucket->slot_size >= kLargeScanAreaThreshold) {
                large_scan_areas_worklist_.Push(
                    {payload_begin, payload_end, slot_span->bucket->slot_size});
              } else {
                scan_areas_worklist_.Push({payload_begin, payload_end});
              }
              return true;
            });
        // If we haven't visited any slot spans, all the slot spans in the
        // super-page are either empty or decommitted. This means that all the
        // objects are freed and there are no quarantined objects.
        if (LIKELY(visited_slot_spans)) {
          super_pages_.insert(reinterpret_cast<uintptr_t>(super_page));
          super_pages_worklist_.Push(reinterpret_cast<uintptr_t>(super_page));
        } else {
#if DCHECK_IS_ON()
          PA_CHECK(IsScannerQuarantineBitmapEmpty(super_page, pcscan_epoch));
#endif
        }
      }
    }
  }
  for (Root* root : pcscan_internal.nonscannable_roots()) {
    typename Root::ScopedGuard guard(root->lock_);
    // Take a snapshot of all super pages and nnonscannable slot spans.
    for (auto* super_page_extent = root->first_extent; super_page_extent;
         super_page_extent = super_page_extent->next) {
      for (char* super_page = super_page_extent->super_page_base;
           super_page != super_page_extent->super_pages_end;
           super_page += kSuperPageSize) {
        super_pages_.insert(reinterpret_cast<uintptr_t>(super_page));
        super_pages_worklist_.Push(reinterpret_cast<uintptr_t>(super_page));
      }
    }
  }
}

}  // namespace

// This class is responsible for performing the entire PCScan task.
// TODO(bikineev): Move PCScan algorithm out of PCScanTask.
class PCScanTask final : public base::RefCountedThreadSafe<PCScanTask> {
 public:
  static void* operator new(size_t size) {
    return PCScanMetadataAllocator().AllocFlagsNoHooks(0, size);
  }

  static void operator delete(void* ptr) {
    PCScanMetadataAllocator().FreeNoHooks(ptr);
  }

  // Creates and initializes a PCScan state.
  explicit PCScanTask(PCScan& pcscan);

  PCScanTask(PCScanTask&&) noexcept = delete;
  PCScanTask& operator=(PCScanTask&&) noexcept = delete;

  // Execute PCScan from mutator inside safepoint.
  void RunFromMutator();

  // Execute PCScan from the scanner thread. Must be called only once from the
  // scanner thread.
  void RunFromScanner();

  PCScanScheduler& scheduler() const { return pcscan_.scheduler(); }

 private:
  class ScanLoop;
  class StackVisitor;

  using Root = PCScan::Root;
  using SlotSpan = SlotSpanMetadata<ThreadSafe>;

  struct GigaCageLookupPolicy {
    ALWAYS_INLINE bool TestOnHeapPointer(uintptr_t maybe_ptr) const {
#if defined(PA_HAS_64_BITS_POINTERS)
#if DCHECK_IS_ON()
      PA_DCHECK(
          IsManagedByPartitionAllocBRPPool(reinterpret_cast<void*>(maybe_ptr)));
#endif
      return QuarantineCardTable::GetFrom(maybe_ptr).IsQuarantined(maybe_ptr);
#else   // defined(PA_HAS_64_BITS_POINTERS)
      return IsManagedByPartitionAllocBRPPool(
          reinterpret_cast<void*>(maybe_ptr));
#endif  // defined(PA_HAS_64_BITS_POINTERS)
    }
    [[maybe_unused]] const PCScanSnapshot& snapshot;
  };

  struct NoGigaCageLookupPolicy {
    ALWAYS_INLINE bool TestOnHeapPointer(uintptr_t maybe_ptr) const {
      const auto super_page_base = maybe_ptr & kSuperPageBaseMask;
      const auto& super_pages = snapshot.quarantinable_super_pages();
      auto it = super_pages.lower_bound(super_page_base);
      return it != super_pages.end() && *it == super_page_base;
    }
    const PCScanSnapshot& snapshot;
  };

  // This is used:
  // - to synchronize all scanning threads (mutators and the scanner);
  // - for the scanner, to transition through the state machine
  //   (kScheduled -> kScanning (ctor) -> kSweepingAndFinishing (dtor).
  template <Context context>
  class SyncScope final {
   public:
    explicit SyncScope(PCScanTask& task) : task_(task) {
      task_.number_of_scanning_threads_.fetch_add(1, std::memory_order_relaxed);
      if (context == Context::kScanner) {
        // Publish the change of the state so that the mutators can join
        // scanning and expect the consistent state.
        task_.pcscan_.state_.store(PCScan::State::kScanning,
                                   std::memory_order_release);
      }
    }
    ~SyncScope() {
      // First, notify other scanning threads that this thread is done.
      NotifyThreads();
      if (context == Context::kScanner) {
        // The scanner thread must wait here until all safepoints leave.
        // Otherwise, sweeping may free a page that can be accessed by a
        // descheduled mutator.
        WaitForOtherThreads();
      }
    }

   private:
    void NotifyThreads() {
      {
        // The lock is required as otherwise there is a race between
        // fetch_sub/notify in the mutator and checking
        // number_of_scanning_threads_/waiting in the scanner.
        std::lock_guard<std::mutex> lock(task_.mutex_);
        task_.number_of_scanning_threads_.fetch_sub(1,
                                                    std::memory_order_relaxed);
        {
          // Notify that scan is done and there is no need to enter
          // the safepoint. This also helps a mutator to avoid repeating
          // entering. Since the scanner thread waits for all threads to finish,
          // there is no ABA problem here. There is technically no need to have
          // CAS here, since |state_| is under the mutex and can only be changed
          // here, but we keep it for safety.
          PCScan::State expected = PCScan::State::kScanning;
          task_.pcscan_.state_.compare_exchange_strong(
              expected, PCScan::State::kSweepingAndFinishing,
              std::memory_order_relaxed, std::memory_order_relaxed);
        }
      }
      task_.condvar_.notify_all();
    }

    void WaitForOtherThreads() {
      std::unique_lock<std::mutex> lock(task_.mutex_);
      task_.condvar_.wait(lock, [this] {
        return !task_.number_of_scanning_threads_.load(
            std::memory_order_relaxed);
      });
    }

    PCScanTask& task_;
  };

  friend class base::RefCountedThreadSafe<PCScanTask>;
  ~PCScanTask() = default;

  template <typename LookupPolicy>
  ALWAYS_INLINE QuarantineBitmap* TryFindScannerBitmapForPointer(
      uintptr_t maybe_ptr) const;

  // Lookup and marking functions. Return size of the object if marked or zero
  // otherwise.
  template <typename LookupPolicy>
  ALWAYS_INLINE size_t TryMarkObjectInNormalBuckets(uintptr_t maybe_ptr) const;

  // Scans stack, only called from safepoints.
  void ScanStack();

  // Scans all registered partitions and marks reachable quarantined objects.
  void ScanPartitions();

  // Clear quarantined objects and prepare card table for fast lookup
  void ClearQuarantinedObjectsAndPrepareCardTable();

  // Sweeps (frees) unreachable quarantined entries. Returns the size of swept
  // objects.
  void SweepQuarantine();

  // Finishes the scanner (updates limits, UMA, etc).
  void FinishScanner();

  // Cache the pcscan epoch to avoid the compiler loading the atomic
  // QuarantineData::epoch_ on each access.
  const size_t pcscan_epoch_;
  PCScanSnapshot snapshot_;
  StatsCollector stats_;
  // Mutex and codvar that are used to synchronize scanning threads.
  std::mutex mutex_;
  std::condition_variable condvar_;
  std::atomic<size_t> number_of_scanning_threads_{0u};
  PCScan& pcscan_;
};

template <typename LookupPolicy>
ALWAYS_INLINE QuarantineBitmap* PCScanTask::TryFindScannerBitmapForPointer(
    uintptr_t maybe_ptr) const {
  // First, check if |maybe_ptr| points to a valid super page or a quarantined
  // card.
  LookupPolicy lookup{snapshot_};
  if (LIKELY(!lookup.TestOnHeapPointer(maybe_ptr)))
    return nullptr;
  // Check if we are not pointing to metadata/guard pages.
  if (!IsWithinSuperPagePayload(reinterpret_cast<char*>(maybe_ptr),
                                true /*with quarantine*/))
    return nullptr;
  // We are certain here that |maybe_ptr| points to the super page payload.
  return QuarantineBitmapFromPointer(QuarantineBitmapType::kScanner,
                                     pcscan_epoch_,
                                     reinterpret_cast<char*>(maybe_ptr));
}

// Looks up and marks a potential dangling pointer. Returns the size of the slot
// (which is then accounted as quarantined) or zero if no object is found.
// For normal bucket super pages, PCScan uses two quarantine bitmaps, the
// mutator and the scanner one. The former is used by mutators when objects are
// freed, while the latter is used concurrently by the PCScan thread. The
// bitmaps are swapped as soon as PCScan is triggered. Once a dangling pointer
// (which points to an object in the scanner bitmap) is found,
// TryMarkObjectInNormalBuckets() marks it again in the bitmap and clears
// from the scanner bitmap. This way, when scanning is done, all uncleared
// entries in the scanner bitmap correspond to unreachable objects.
template <typename LookupPolicy>
ALWAYS_INLINE size_t
PCScanTask::TryMarkObjectInNormalBuckets(uintptr_t maybe_ptr) const {
  // TODO(bartekn): Add a "is in normal buckets" DCHECK in the |else| case.
  using AccessType = QuarantineBitmap::AccessType;
  // Check if |maybe_ptr| points somewhere to the heap.
  auto* scanner_bitmap =
      TryFindScannerBitmapForPointer<LookupPolicy>(maybe_ptr);
  if (!scanner_bitmap)
    return 0;

  auto* root =
      Root::FromPointerInNormalBuckets(reinterpret_cast<char*>(maybe_ptr));

  // Check if pointer was in the quarantine bitmap.
  const uintptr_t base = GetObjectStartInSuperPage(maybe_ptr, *root);
  if (!base || !scanner_bitmap->template CheckBit<AccessType::kAtomic>(base))
    return 0;

  PA_DCHECK((maybe_ptr & kSuperPageBaseMask) == (base & kSuperPageBaseMask));

  auto* target_slot_span =
      SlotSpan::FromSlotInnerPtr(reinterpret_cast<void*>(base));
  PA_DCHECK(root == Root::FromSlotSpan(target_slot_span));

  const size_t usable_size = target_slot_span->GetUsableSize(root);
  // Range check for inner pointers.
  if (maybe_ptr >= base + usable_size)
    return 0;

  // Now we are certain that |maybe_ptr| is a dangling pointer. Mark it again in
  // the mutator bitmap and clear from the scanner bitmap. Note that since
  // PCScan has exclusive access to the scanner bitmap, we can avoid atomic rmw
  // operation for it.
  scanner_bitmap->template ClearBit<AccessType::kAtomic>(base);
  QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator, pcscan_epoch_,
                              reinterpret_cast<char*>(base))
      ->template SetBit<AccessType::kAtomic>(base);
  return target_slot_span->bucket->slot_size;
}

void PCScanTask::ClearQuarantinedObjectsAndPrepareCardTable() {
  using AccessType = QuarantineBitmap::AccessType;

  const bool giga_cage_enabled = features::IsPartitionAllocGigaCageEnabled();
  PCScanSnapshot::SuperPagesWorklist::RandomizedView super_pages(
      snapshot_.quarantinable_super_pages_worklist());
  super_pages.Visit([this, giga_cage_enabled](uintptr_t super_page_base) {
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_epoch_,
        reinterpret_cast<char*>(super_page_base));
    auto* root = Root::FromSuperPage(reinterpret_cast<char*>(super_page_base));
    bitmap->template Iterate<AccessType::kNonAtomic>(
        [root, giga_cage_enabled](uintptr_t ptr) {
          auto* object = reinterpret_cast<void*>(ptr);
          auto* slot_span = SlotSpan::FromSlotInnerPtr(object);
          // Use zero as a zapping value to speed up the fast bailout check in
          // ScanPartitions.
          const size_t size = slot_span->GetUsableSize(root);
          memset(object, 0, size);
#if defined(PA_HAS_64_BITS_POINTERS)
          if (giga_cage_enabled) {
            // Set card(s) for this quarantined object.
            QuarantineCardTable::GetFrom(ptr).Quarantine(ptr, size);
          }
#else
          (void)giga_cage_enabled;
#endif
        });
  });
}

// Class used to perform actual scanning. Dispatches at runtime based on
// supported SIMD extensions.
class PCScanTask::ScanLoop final {
 public:
  explicit ScanLoop(const PCScanTask& pcscan_task)
      : scan_function_(GetScanFunction()),
        pcscan_task_(pcscan_task)
#if defined(PA_HAS_64_BITS_POINTERS)
        ,
        brp_pool_base_(PartitionAddressSpace::BRPPoolBase())
#endif
  {
  }

  ScanLoop(const ScanLoop&) = delete;
  ScanLoop& operator=(const ScanLoop&) = delete;

  // Scans a range of addresses and marks reachable quarantined objects. Returns
  // the size of marked objects. The function race-fully reads the heap and
  // therefore TSAN is disabled for the dispatch functions.
  size_t Run(uintptr_t* begin, uintptr_t* end) const {
    static_assert(alignof(uintptr_t) % alignof(void*) == 0,
                  "Alignment of uintptr_t must be at least as strict as "
                  "alignment of a pointer type.");
    return (this->*scan_function_)(begin, end);
  }

 private:
  // This is to support polymorphic behavior and to avoid virtual calls.
  using ScanFunction = size_t (ScanLoop::*)(uintptr_t*, uintptr_t*) const;

  static ScanFunction GetScanFunction() {
    if (UNLIKELY(!features::IsPartitionAllocGigaCageEnabled())) {
      return &ScanLoop::RunUnvectorizedNoGigaCage;
    }
// We allow vectorization only for 64bit since they require support of the
// 64bit GigaCage, and only for x86 because a special instruction set is
// required.
#if defined(ARCH_CPU_X86_64)
    const SimdSupport simd = PCScanInternal::Instance().simd_support();
    if (simd == SimdSupport::kAVX2)
      return &ScanLoop::RunAVX2;
    if (simd == SimdSupport::kSSE41)
      return &ScanLoop::RunSSE4;
#endif
    return &ScanLoop::RunUnvectorized;
  }

#if defined(PA_HAS_64_BITS_POINTERS)
  ALWAYS_INLINE bool IsInBRPPool(uintptr_t maybe_ptr) const {
    return (maybe_ptr & PartitionAddressSpace::BRPPoolBaseMask()) ==
           brp_pool_base_;
  }
#endif

#if defined(ARCH_CPU_X86_64)
  __attribute__((target("sse4.1"))) NO_SANITIZE("thread") size_t
      RunSSE4(uintptr_t* begin, uintptr_t* end) const {
    static constexpr size_t kAlignmentRequirement = 16;
    static constexpr size_t kWordsInVector = 2;
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
    PA_DCHECK(
        !((reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin)) %
          kAlignmentRequirement));
    const __m128i vbase = _mm_set1_epi64x(brp_pool_base_);
    const __m128i cage_mask =
        _mm_set1_epi64x(PartitionAddressSpace::BRPPoolBaseMask());

    size_t quarantine_size = 0;
    for (uintptr_t* payload = begin; payload < end; payload += kWordsInVector) {
      const __m128i maybe_ptrs =
          _mm_loadu_si128(reinterpret_cast<__m128i*>(payload));
      const __m128i vand = _mm_and_si128(maybe_ptrs, cage_mask);
      const __m128i vcmp = _mm_cmpeq_epi64(vand, vbase);
      const int mask = _mm_movemask_pd(_mm_castsi128_pd(vcmp));
      if (LIKELY(!mask))
        continue;
      // It's important to extract pointers from the already loaded vector to
      // avoid racing with the mutator.
      // Once BRP check passes, we know we're dealing with normal buckets.
      if (mask & 0b01) {
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
                _mm_cvtsi128_si64(maybe_ptrs));
      }
      if (mask & 0b10) {
        // Extraction intrinsics for qwords are only supported in SSE4.1, so
        // instead we reshuffle dwords with pshufd. The mask is used to move the
        // 4th and 3rd dwords into the second and first position.
        static constexpr int kSecondWordMask = (3 << 2) | (2 << 0);
        const __m128i shuffled = _mm_shuffle_epi32(maybe_ptrs, kSecondWordMask);
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
                _mm_cvtsi128_si64(shuffled));
      }
    }
    return quarantine_size;
  }

  __attribute__((target("avx2"))) NO_SANITIZE("thread") size_t
      RunAVX2(uintptr_t* begin, uintptr_t* end) const {
    static constexpr size_t kAlignmentRequirement = 32;
    static constexpr size_t kWordsInVector = 4;
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
    // For AVX2, stick to integer instructions. This brings slightly better
    // throughput. For example, according to the Intel docs, on Broadwell and
    // Haswell the CPI of vmovdqa (_mm256_load_si256) is twice smaller (0.25)
    // than that of vmovapd (_mm256_load_pd).
    const __m256i vbase = _mm256_set1_epi64x(brp_pool_base_);
    const __m256i cage_mask =
        _mm256_set1_epi64x(PartitionAddressSpace::BRPPoolBaseMask());

    size_t quarantine_size = 0;
    uintptr_t* payload = begin;
    for (; payload < (end - kWordsInVector); payload += kWordsInVector) {
      const __m256i maybe_ptrs =
          _mm256_load_si256(reinterpret_cast<__m256i*>(payload));
      const __m256i vand = _mm256_and_si256(maybe_ptrs, cage_mask);
      const __m256i vcmp = _mm256_cmpeq_epi64(vand, vbase);
      const int mask = _mm256_movemask_pd(_mm256_castsi256_pd(vcmp));
      if (LIKELY(!mask))
        continue;
      // It's important to extract pointers from the already loaded vector to
      // avoid racing with the mutator.
      // Once BRP check passes, we know we're dealing with normal buckets.
      if (mask & 0b0001)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 0));
      if (mask & 0b0010)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 1));
      if (mask & 0b0100)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 2));
      if (mask & 0b1000)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 3));
    }

    quarantine_size += RunUnvectorized(payload, end);
    return quarantine_size;
  }
#endif  // defined(ARCH_CPU_X86_64)

  ALWAYS_INLINE NO_SANITIZE("thread") size_t
      RunUnvectorized(uintptr_t* begin, uintptr_t* end) const {
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t)));
    size_t quarantine_size = 0;
    for (; begin < end; ++begin) {
      uintptr_t maybe_ptr = *begin;
#if defined(PA_HAS_64_BITS_POINTERS)
      // On 64bit architectures, call IsInBRPPool instead of
      // IsManagedByPartitionAllocBRPPool to avoid redundant loads of
      // PartitionAddressSpace::brp_pool_base_address_.
      if (LIKELY(!IsInBRPPool(maybe_ptr)))
        continue;
        // Once BRP check passes, we know we're dealing with normal buckets.
#endif
      quarantine_size +=
          pcscan_task_.TryMarkObjectInNormalBuckets<GigaCageLookupPolicy>(
              maybe_ptr);
    }
    return quarantine_size;
  }

  ALWAYS_INLINE NO_SANITIZE("thread") size_t
      RunUnvectorizedNoGigaCage(uintptr_t* begin, uintptr_t* end) const {
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t)));
    size_t quarantine_size = 0;
    for (; begin < end; ++begin) {
      uintptr_t maybe_ptr = *begin;
      if (!maybe_ptr)
        continue;
      quarantine_size +=
          pcscan_task_.TryMarkObjectInNormalBuckets<NoGigaCageLookupPolicy>(
              maybe_ptr);
    }
    return quarantine_size;
  }

  const ScanFunction scan_function_;
  const PCScanTask& pcscan_task_;
#if defined(PA_HAS_64_BITS_POINTERS)
  // Keep this a constant so that the compiler can remove redundant loads for
  // the base of the BRP pool and hoist them out of the loops.
  const uintptr_t brp_pool_base_;
#endif
};

class PCScanTask::StackVisitor final : public internal::StackVisitor {
 public:
  explicit StackVisitor(const PCScanTask& task) : task_(task) {}

  void VisitStack(uintptr_t* stack_ptr, uintptr_t* stack_top) override {
    static constexpr size_t kMinimalAlignment = 32;
    stack_ptr = reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(stack_ptr) & ~(kMinimalAlignment - 1));
    stack_top = reinterpret_cast<uintptr_t*>(
        (reinterpret_cast<uintptr_t>(stack_top) + kMinimalAlignment - 1) &
        ~(kMinimalAlignment - 1));
    PA_CHECK(stack_ptr < stack_top);
    ScanLoop loop(task_);
    quarantine_size_ += loop.Run(stack_ptr, stack_top);
  }

  // Returns size of quarantined objects that are reachable from the current
  // stack.
  size_t quarantine_size() const { return quarantine_size_; }

 private:
  const PCScanTask& task_;
  size_t quarantine_size_ = 0;
};

PCScanTask::PCScanTask(PCScan& pcscan)
    : pcscan_epoch_(pcscan.epoch()),
      stats_(PCScanInternal::Instance().process_name()),
      pcscan_(pcscan) {}

void PCScanTask::ScanStack() {
  const auto& pcscan = PCScanInternal::Instance();
  if (!pcscan.IsStackScanningEnabled())
    return;
  // Check if the stack top was registered. It may happen that it's not if the
  // current allocation happens from pthread trampolines.
  void* stack_top = pcscan.GetCurrentThreadStackTop();
  if (UNLIKELY(!stack_top))
    return;

  Stack stack_scanner(stack_top);
  StackVisitor visitor(*this);
  stack_scanner.IteratePointers(&visitor);
  stats_.IncreaseSurvivedQuarantineSize(visitor.quarantine_size());
}

void PCScanTask::ScanPartitions() {
  const ScanLoop scan_loop(*this);
  // For scanning large areas, it's worthwhile checking whether the range that
  // is scanned contains quarantined objects.
  size_t quarantine_size = 0;

  // Scan areas with large slots.
  PCScanSnapshot::LargeScanAreasWorklist::RandomizedView large_scan_areas(
      snapshot_.large_scan_areas_worklist());
  large_scan_areas.Visit([this, &scan_loop, &quarantine_size](auto scan_area) {
    // The bitmap is (a) always guaranteed to exist and (b) the same for all
    // objects in a given slot span.
    // TODO(chromium:1129751): Check mutator bitmap as well if performance
    // allows.
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_epoch_,
        reinterpret_cast<char*>(scan_area.begin));
    for (uintptr_t* current_slot = scan_area.begin;
         current_slot < scan_area.end;
         current_slot += (scan_area.slot_size / sizeof(uintptr_t))) {
      // It is okay to skip objects as their payload has been zapped at this
      // point which means that the pointers no longer retain other objects.
      if (bitmap->CheckBit(reinterpret_cast<uintptr_t>(current_slot))) {
        continue;
      }
      uintptr_t* current_slot_end =
          current_slot + (scan_area.slot_size / sizeof(uintptr_t));
      PA_DCHECK(current_slot_end <= scan_area.end);
      quarantine_size += scan_loop.Run(current_slot, current_slot_end);
    }
  });

  // Scan areas with regular size slots.
  PCScanSnapshot::ScanAreasWorklist::RandomizedView scan_areas(
      snapshot_.scan_areas_worklist());
  scan_areas.Visit([&scan_loop, &quarantine_size](auto scan_area) {
    quarantine_size += scan_loop.Run(scan_area.begin, scan_area.end);
  });

  stats_.IncreaseSurvivedQuarantineSize(quarantine_size);
}

void PCScanTask::SweepQuarantine() {
  using AccessType = QuarantineBitmap::AccessType;

  const bool giga_cage_enabled = features::IsPartitionAllocGigaCageEnabled();
  size_t swept_bytes = 0;

  for (uintptr_t super_page : snapshot_.quarantinable_super_pages()) {
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_epoch_,
        reinterpret_cast<char*>(super_page));
    auto* root = Root::FromSuperPage(reinterpret_cast<char*>(super_page));
    bitmap->template IterateAndClear<AccessType::kNonAtomic>(
        [root, giga_cage_enabled, &swept_bytes](uintptr_t ptr) {
          auto* object = reinterpret_cast<void*>(ptr);
          auto* slot_span = SlotSpan::FromSlotInnerPtr(object);
          swept_bytes += slot_span->bucket->slot_size;
          root->FreeNoHooksImmediate(object, slot_span);
#if defined(PA_HAS_64_BITS_POINTERS)
          if (giga_cage_enabled) {
            // Reset card(s) for this quarantined object. Please note that the
            // cards may still contain quarantined objects (which were promoted
            // in this scan cycle), but
            // ClearQuarantinedObjectsAndFilterSuperPages() will set them again
            // in the next PCScan cycle.
            QuarantineCardTable::GetFrom(ptr).Unquarantine(
                ptr, slot_span->GetUsableSize(root));
          }
#else
          (void)giga_cage_enabled;
#endif
        });
  }

  stats_.IncreaseSweptSize(swept_bytes);

#if defined(PA_THREAD_CACHE_SUPPORTED)
  // Sweeping potentially frees into the current thread's thread cache. Purge
  // releases the cache back to the global allocator.
  auto* current_thread_tcache = ThreadCache::Get();
  if (ThreadCache::IsValid(current_thread_tcache))
    current_thread_tcache->Purge();
#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

void PCScanTask::FinishScanner() {
  stats_.ReportTracesAndHists();
  LogStats(
      stats_.swept_size(),
      pcscan_.scheduler_.scheduling_backend().GetQuarantineData().last_size,
      stats_.survived_quarantine_size());

  pcscan_.scheduler_.scheduling_backend().UpdateScheduleAfterScan(
      stats_.survived_quarantine_size(), stats_.GetOverallTime(),
      PCScanInternal::Instance().CalculateTotalHeapSize());

  PCScanInternal::Instance().ResetCurrentPCScanTask();
  // Check that concurrent task can't be scheduled twice.
  PA_CHECK(pcscan_.state_.exchange(PCScan::State::kNotRunning,
                                   std::memory_order_acq_rel) ==
           PCScan::State::kSweepingAndFinishing);
}

void PCScanTask::RunFromMutator() {
  ReentrantScannerGuard reentrancy_guard;
  StatsCollector::MutatorScope overall_scope(
      stats_, StatsCollector::MutatorId::kOverall);
  {
    SyncScope<Context::kMutator> sync_scope(*this);
    // Mutator might start entering the safepoint while scanning was already
    // finished.
    if (!pcscan_.IsJoinable())
      return;
    // Take snapshot of partition-alloc heap if not yet taken.
    snapshot_.EnsureTaken(pcscan_epoch_);
    {
      // Clear all quarantined objects and prepare card table.
      StatsCollector::MutatorScope clear_scope(
          stats_, StatsCollector::MutatorId::kClear);
      ClearQuarantinedObjectsAndPrepareCardTable();
    }
    {
      // Scan the thread's stack to find dangling references.
      StatsCollector::MutatorScope scan_scope(
          stats_, StatsCollector::MutatorId::kScanStack);
      ScanStack();
    }
    {
      // Scan heap for dangling references.
      StatsCollector::MutatorScope scan_scope(stats_,
                                              StatsCollector::MutatorId::kScan);
      ScanPartitions();
    }
  }
}

void PCScanTask::RunFromScanner() {
  ReentrantScannerGuard reentrancy_guard;
  {
    StatsCollector::ScannerScope overall_scope(
        stats_, StatsCollector::ScannerId::kOverall);
    {
      SyncScope<Context::kScanner> sync_scope(*this);
      // Take snapshot of partition-alloc heap.
      snapshot_.EnsureTaken(pcscan_epoch_);
      {
        // Clear all quarantined objects and prepare the card table.
        StatsCollector::ScannerScope clear_scope(
            stats_, StatsCollector::ScannerId::kClear);
        ClearQuarantinedObjectsAndPrepareCardTable();
      }
      {
        // Scan heap for dangling references.
        StatsCollector::ScannerScope scan_scope(
            stats_, StatsCollector::ScannerId::kScan);
        ScanPartitions();
      }
    }
    {
      // Sweep unreachable quarantined objects.
      StatsCollector::ScannerScope sweep_scope(
          stats_, StatsCollector::ScannerId::kSweep);
      SweepQuarantine();
    }
  }
  FinishScanner();
}

class PCScan::PCScanThread final {
 public:
  using TaskHandle = PCScanInternal::TaskHandle;

  static PCScanThread& Instance() {
    // Lazily instantiate the scanning thread.
    static base::NoDestructor<PCScanThread> instance;
    return *instance;
  }

  void PostTask(TaskHandle task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      PA_DCHECK(!posted_task_.get());
      posted_task_ = std::move(task);
      wanted_delay_ = TimeDelta();
    }
    condvar_.notify_one();
  }

  void PostDelayedTask(TimeDelta delay) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (posted_task_.get()) {
        return;
      }
      wanted_delay_ = delay;
    }
    condvar_.notify_one();
  }

 private:
  friend class base::NoDestructor<PCScanThread>;

  PCScanThread() {
    std::thread{[this] {
      static constexpr const char* kThreadName = "PCScan";
      // Ideally we should avoid mixing base:: and std:: API for threading, but
      // this is useful for visualizing the pcscan thread in chrome://tracing.
      base::PlatformThread::SetName(kThreadName);
      TaskLoop();
    }}.detach();
  }

  // Waits and returns whether the delay should be recomputed.
  bool Wait(std::unique_lock<std::mutex>& lock) {
    PA_DCHECK(lock.owns_lock());
    if (wanted_delay_.is_zero()) {
      condvar_.wait(lock, [this] {
        // Re-evaluate if either delay changed, or a task was
        // enqueued.
        return !wanted_delay_.is_zero() || posted_task_.get();
      });
      // The delay has already been set up and should not be queried again.
      return false;
    }
    condvar_.wait_for(
        lock, std::chrono::microseconds(wanted_delay_.InMicroseconds()));
    // If no task has been posted, the delay should be recomputed at this point.
    return !posted_task_.get();
  }

  void TaskLoop() {
    while (true) {
      TaskHandle current_task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        // Scheduling.
        while (!posted_task_.get()) {
          if (Wait(lock)) {
            wanted_delay_ =
                scheduler().scheduling_backend().UpdateDelayedSchedule();
            if (wanted_delay_.is_zero()) {
              break;
            }
          }
        }
        // Differentiate between a posted task and a delayed task schedule.
        if (posted_task_.get()) {
          std::swap(current_task, posted_task_);
          wanted_delay_ = TimeDelta();
        } else {
          PA_DCHECK(wanted_delay_.is_zero());
        }
      }
      // Differentiate between a posted task and a delayed task schedule.
      if (current_task.get()) {
        current_task->RunFromScanner();
      } else {
        PCScan::Instance().PerformScan(PCScan::InvocationMode::kNonBlocking);
      }
    }
  }

  PCScanScheduler& scheduler() const { return PCScan::Instance().scheduler(); }

  std::mutex mutex_;
  std::condition_variable condvar_;
  TaskHandle posted_task_;
  TimeDelta wanted_delay_;
};

void PCScan::PerformScan(InvocationMode invocation_mode) {
#if DCHECK_IS_ON()
  const auto& internal = PCScanInternal::Instance();
  const auto& scannable_roots = internal.scannable_roots();
  const auto& nonscannable_roots = internal.nonscannable_roots();
  PA_DCHECK(scannable_roots.size() > 0);
  PA_DCHECK(std::all_of(scannable_roots.begin(), scannable_roots.end(),
                        [](Root* root) { return root->IsScanEnabled(); }));
  PA_DCHECK(
      std::all_of(nonscannable_roots.begin(), nonscannable_roots.end(),
                  [](Root* root) { return root->IsQuarantineEnabled(); }));
#endif

  {
    // If scanning is already in progress, bail out.
    State expected = State::kNotRunning;
    if (!state_.compare_exchange_strong(expected, State::kScheduled,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed))
      return;
  }

  scheduler_.scheduling_backend().ScanStarted();

  // Create PCScan task and set it as current.
  auto task = base::MakeRefCounted<PCScanTask>(*this);
  PCScanInternal::Instance().SetCurrentPCScanTask(task);

  if (UNLIKELY(invocation_mode == InvocationMode::kScheduleOnlyForTesting)) {
    // Immediately change the state to enable safepoint testing.
    state_.store(State::kScanning, std::memory_order_release);
    return;
  }

  // Post PCScan task.
  if (LIKELY(invocation_mode == InvocationMode::kNonBlocking)) {
    PCScanThread::Instance().PostTask(std::move(task));
  } else {
    PA_DCHECK(InvocationMode::kBlocking == invocation_mode ||
              InvocationMode::kForcedBlocking == invocation_mode);
    std::move(*task).RunFromScanner();
  }
}

void PCScan::PerformScanIfNeeded(InvocationMode invocation_mode) {
  if (!PCScanInternal::Instance().scannable_roots().size())
    return;
  PCScan& instance = Instance();
  if (invocation_mode == InvocationMode::kForcedBlocking ||
      instance.scheduler_.scheduling_backend()
          .GetQuarantineData()
          .MinimumScanningThresholdReached())
    instance.PerformScan(invocation_mode);
}

void PCScan::PerformDelayedScan(TimeDelta delay) {
  PCScanThread::Instance().PostDelayedTask(delay);
}

void PCScan::JoinScan() {
#if !PCSCAN_DISABLE_SAFEPOINTS
  auto& internal = PCScanInternal::Instance();
  // Current task can be destroyed by the scanner. Check that it's valid.
  if (auto current_task = internal.CurrentPCScanTask())
    current_task->RunFromMutator();
#endif
}

void PCScan::FinishScanForTesting() {
  auto& internal = PCScanInternal::Instance();
  auto current_task = internal.CurrentPCScanTask();
  PA_CHECK(current_task.get());
  current_task->RunFromScanner();
}

void PCScan::RegisterScannableRoot(Root* root) {
  PCScanInternal::Instance().RegisterScannableRoot(root);
}

void PCScan::RegisterNonScannableRoot(Root* root) {
  PCScanInternal::Instance().RegisterNonScannableRoot(root);
}

void PCScan::SetProcessName(const char* process_name) {
  PCScanInternal::Instance().SetProcessName(process_name);
}

void PCScan::EnableStackScanning() {
  PCScanInternal::Instance().EnableStackScanning();
}
void PCScan::DisableStackScanning() {
  PCScanInternal::Instance().DisableStackScanning();
}
bool PCScan::IsStackScanningEnabled() {
  return PCScanInternal::Instance().IsStackScanningEnabled();
}

void PCScan::NotifyThreadCreated(void* stack_top) {
  PCScanInternal::Instance().NotifyThreadCreated(stack_top);
}
void PCScan::NotifyThreadDestroyed() {
  PCScanInternal::Instance().NotifyThreadDestroyed();
}

void PCScan::UninitForTesting() {
  PCScanInternal::Instance().ClearRootsForTesting();  // IN-TEST
  ReinitPCScanMetadataAllocatorForTesting();          // IN-TEST
}

void PCScan::ReinitForTesting() {
  PCScanInternal::Instance().ReinitForTesting();  // IN-TEST
}

PCScan PCScan::instance_ PA_CONSTINIT;

}  // namespace internal
}  // namespace base
