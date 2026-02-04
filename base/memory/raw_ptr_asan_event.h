// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_ASAN_EVENT_H_
#define BASE_MEMORY_RAW_PTR_ASAN_EVENT_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#include <array>

#include "base/memory/raw_ptr_asan_allocator.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "partition_alloc/partition_lock.h"

namespace base::internal {

template <typename T>
struct RawPtrAsanAllocator;
template <typename T>
using RawPtrAsanVector = std::vector<T, RawPtrAsanAllocator<T>>;

// RawPtrAsanThreadId is used inside MallocHook(). So if any memory allocation,
// e.g. malloc() or tls-alloc() or mmap(), ... is required to obtain the id,
// it will cause stack-overflow because the memory allocation is hooked by
// ASAN and ASAN invokes MallocHook() again.
// If using `PlatformThreadId`, i.e. PlatformThread::CurrentId(), it seems to
// have 2 problems. One is depending on syscall(__NR_gettid). syscall() is
// very slow. The other is depending on `thread_local` to avoid slow syscall()
// multiple times. i.e. `thread_local pid_t g_thread_id`. `thread_local` may
// causes memory allocation.
// If using `SequenceToken`, `SequenceToken::GetForCurrentThread()` and
// `internal::ThreadGroup::CurrentThreadHasGroup()` will cause the problem,
// because the both methods depends on `thread_local` variables, i.e.
// `current_sequence_token` and `current_thread_group`.
using RawPtrAsanThreadId = PlatformThreadRef;

inline RawPtrAsanThreadId GetCurrentRawPtrAsanThreadId() {
  return PlatformThreadBase::CurrentRef();
}

// We collect a log of "relevant" events at runtime, and then either when a
// fatal crash occurs or at process exit we can process this log and determine
// whether any events occurred that should be reported, and whether those events
// should be considered to be protected by MiraclePtr.
//
// RawPtrAsanEvent is the type used to store these events, along with metadata
// such as the stack trace when the event occurred.
struct RawPtrAsanEvent {
  enum class Type : uint8_t {
    kQuarantineEntry,
    kQuarantineAssignment,
    kQuarantineRead,
    kQuarantineWrite,
    kQuarantineExit,
    kFreeAssignment,
  };

  // TODO(crbug.com/447520906): base::debug::AsanService::Log() causes memory
  // allocation/deallocation because it depends std::string. The method must
  // not be used inside malloc_hook(), ignore_free_hook() and also free_hook().
  // So the following methods: PrintEvent() and PrintEventStack() depend on
  // AsanService::Log(), we must not use them inside the hooks. We will solve
  // the Log()'s memory allocation issue later, e.g. use PartitionAlloc instead.
  void PrintEvent(bool print_stack) const;
  void PrintEventStack() const;

  bool IsSameAllocation(const RawPtrAsanEvent& other) const {
    return (address <= other.address && other.address <= address + size) ||
           (other.address <= address && address <= other.address + other.size);
  }

  uintptr_t fault_address;
  Type type;
  RawPtrAsanThreadId thread_id;
  uintptr_t address;
  size_t size;
  std::array<const void*, 12> stack;
};

// Since RawPtrAsanService is statically initialized, and we need to be able
// to access it in extremely hot paths, we move the logging into a separate
// class.
class RawPtrAsanEventLog {
 public:
  RawPtrAsanEventLog();
  ~RawPtrAsanEventLog();

  void Add(RawPtrAsanEvent&& event) LOCKS_EXCLUDED(GetLock());
  void Print(bool print_stack) LOCKS_EXCLUDED(GetLock());

  void ClearForTesting() LOCKS_EXCLUDED(GetLock()) {  // IN-TEST
    internal::PartitionAutoLock lock(GetLock());
    events_.clear();
  }

  internal::PartitionLock& GetLock() { return lock_; }

  RawPtrAsanVector<RawPtrAsanEvent>& events()
      EXCLUSIVE_LOCKS_REQUIRED(GetLock()) {
    return events_;
  }

 private:
  internal::PartitionLock lock_;
  RawPtrAsanVector<RawPtrAsanEvent> events_ GUARDED_BY(GetLock());
};

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#endif  // BASE_MEMORY_RAW_PTR_ASAN_EVENT_H_
