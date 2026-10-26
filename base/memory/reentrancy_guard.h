// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_REENTRANCY_GUARD_H_
#define BASE_MEMORY_REENTRANCY_GUARD_H_

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && \
    (PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_WIN))

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "partition_alloc/partition_alloc_base/scoped_clear_last_error.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_ref.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_lock.h"

#if PA_BUILDFLAG(IS_POSIX)
#include <pthread.h>
#endif  // PA_BUILDFLAG(IS_POSIX)

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // PA_BUILDFLAG(IS_WIN)

#include <array>
#include <atomic>
#include <cstdint>

#include "partition_alloc/yield_processor.h"

namespace base::internal {

struct BASE_EXPORT ReentrancyState {
  using PlatformThreadRef = partition_alloc::internal::base::PlatformThreadRef;
  using PlatformThread = partition_alloc::internal::base::PlatformThread;

#if PA_BUILDFLAG(IS_POSIX)
  using TlsKey = pthread_key_t;
#elif PA_BUILDFLAG(IS_WIN)
  using TlsKey = unsigned long;
#else
  static_assert(false, "Not supported platform.");
#endif
  static inline constexpr TlsKey kInvalidTlsKey = static_cast<TlsKey>(-1);

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  struct FastHasher {
    NO_SANITIZE("address")
    size_t operator()(uintptr_t value) const {
      return base::FastHash(base::byte_span_from_ref(value));
    }
  };

  struct BASE_EXPORT Guard {
    inline static constexpr size_t kCapacity = 64u;
    inline static constexpr size_t kMask = kCapacity - 1u;
    inline static constexpr size_t kMaxProbes = 8u;

    NO_SANITIZE("address")
    PA_ALWAYS_INLINE static bool AreEnabled() {
      return guard_count_.load(std::memory_order_relaxed) > 0;
    }

    NO_SANITIZE("address")
    PA_ALWAYS_INLINE static bool Contains(const PlatformThreadRef thread_ref) {
      size_t start = thread_ref.hash_value<FastHasher>() & kMask;
      for (size_t i = 0; i < kMaxProbes; ++i) {
        size_t idx = (start + i) & kMask;
        PlatformThreadRef entry = threads_[idx].load(std::memory_order_acquire);
        if (entry == thread_ref) {
          return true;
        }
      }
      return false;
    }

    NO_SANITIZE("address")
    explicit Guard(PlatformThreadRef thread_ref) : thread_ref_(thread_ref) {
      size_t start = thread_ref.hash_value<FastHasher>() & kMask;
      while (true) {
        for (size_t i = 0; i < kMaxProbes; ++i) {
          size_t idx = (start + i) & kMask;
          std::atomic<PlatformThreadRef>& entry = threads_[idx];
          PlatformThreadRef expected;
          if (entry.compare_exchange_strong(expected, thread_ref_,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
            index_ = idx;
            guard_count_.fetch_add(1, std::memory_order_relaxed);
            return;
          }
        }
        // Extremely rare fallback under pathological saturation: yield and
        // retry.
        PA_YIELD_PROCESSOR;
      }
    }

    NO_SANITIZE("address")
    Guard() : Guard(PlatformThread::CurrentRef()) {}

    NO_SANITIZE("address")
    ~Guard() {
      PA_DCHECK(index_ < kCapacity);
      threads_[index_].store(PlatformThreadRef(), std::memory_order_release);
      guard_count_.fetch_sub(1, std::memory_order_relaxed);
    }

   private:
    PlatformThreadRef thread_ref_;
    size_t index_ = kCapacity;
    static std::atomic<int> guard_count_;
    static std::array<std::atomic<PlatformThreadRef>, kCapacity> threads_;
  };
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

  enum class Value : uintptr_t {
    kUninitialized = 0,  // the initial value.
    kInitialized,
    kInsideShim,
  };

  NO_SANITIZE("address")
  static void InitializeKey();

  static void ResetKeyForTesting();

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static void Initialize() {
    return Initialize(PlatformThread::CurrentRef());
  }

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static void Initialize(PlatformThreadRef thread_ref) {
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
    // (1) this thread has not initialized the thread local variable yet.
    // The first pthread_setspecific() may cause memory allocation.
    Guard guard_scope(thread_ref);
#endif
    SetState(Value::kInitialized);
  }

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static Value GetState() {
#if PA_BUILDFLAG(IS_POSIX)
    auto* value = pthread_getspecific(key_);
#endif
#if PA_BUILDFLAG(IS_WIN)
    auto* value = TlsGetValue(key_);
#endif
    return static_cast<Value>(reinterpret_cast<uintptr_t>(value));
  }

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static void SetState(Value state) {
#if PA_BUILDFLAG(IS_POSIX)
    [[maybe_unused]] int error =
        pthread_setspecific(key_, reinterpret_cast<const void*>(state));
    PA_DCHECK(error == 0);
#endif
#if PA_BUILDFLAG(IS_WIN)
    [[maybe_unused]] int ok = TlsSetValue(key_, reinterpret_cast<void*>(state));
    PA_DCHECK(ok);
#endif
  }

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static bool IsInsideShim() {
    return GetState() == Value::kInsideShim;
  }

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static void EnterInsideShim() {
    PA_DCHECK(GetState() == Value::kInitialized);
    SetState(Value::kInsideShim);
  }

  template <bool allow_nullptr>
  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static void LeaveInsideShim() {
    auto state = GetState();
    PA_DCHECK(state == Value::kInsideShim ||
              (allow_nullptr && state == Value::kUninitialized));
    if constexpr (allow_nullptr) {
      if (state == Value::kUninitialized) [[unlikely]] {
        return;
      }
    }
    SetState(Value::kInitialized);
  }

  NO_SANITIZE("address")
  PA_ALWAYS_INLINE static bool TryEnter() {
    auto reentrancy_guard_state = GetState();
    if (reentrancy_guard_state != Value::kInitialized) [[unlikely]] {
      if (reentrancy_guard_state == Value::kUninitialized) [[likely]] {
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
        // `kUninitialized` means (1) this thread has not initialized the thread
        // local variable yet or (2) pthread_setspecific() for the
        // initialization causes memory allocation and reentrancy.
        auto thread_ref =
            partition_alloc::internal::base::PlatformThread::CurrentRef();
        if (Guard::AreEnabled() && Guard::Contains(thread_ref)) [[unlikely]] {
          // (2) pthread_setspecific() caused this reentrancy.
          return false;
        }
        Initialize(thread_ref);
#else   // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
        Initialize();
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
      } else {
        PA_DCHECK(reentrancy_guard_state == Value::kInsideShim);
        // Reentrancy issue, which is not caused by the thread local variable,
        // is detected. `dispatch` caused memory allocation or deallocation
        // internally.
        return false;
      }
    }
    return true;
  }

  struct Scope {
    NO_SANITIZE("address")
    Scope() { EnterInsideShim(); }
    NO_SANITIZE("address")
    ~Scope() { LeaveInsideShim<false>(); }
  };

  // free() is invoked while terminating current thread. So the thread local
  // variable may be destroyed between Scope::Scope() and Scope::~Scope(). In
  // the case, pthread_getspecific() at Scope::~Scope() returns `nullptr`.
  // LeaveInsideShim() at free() needs to allow `nullptr`.
  struct ScopeAtFree {
    NO_SANITIZE("address")
    ScopeAtFree() { EnterInsideShim(); }
    NO_SANITIZE("address")
    ~ScopeAtFree() { LeaveInsideShim<true>(); }
  };

 private:
  static TlsKey key_;
  static partition_alloc::internal::Lock creation_lock_;
  static bool key_created_ PA_GUARDED_BY(creation_lock_);
};

}  // namespace base::internal

#if !PA_BUILDFLAG(IS_WIN)

#define REENTRANCY_GUARD_COMMON(...)                               \
  if (!base::internal::ReentrancyState::TryEnter()) [[unlikely]] { \
    return __VA_ARGS__;                                            \
  }                                                                \
  static_assert(true)

#else  // !PA_BUILDFLAG(IS_WIN)

// Since TlsSetValue() and TlsGetValue() change GetLastError(), need to add
// `ScopedClearLastError` here.
#define REENTRANCY_GUARD_COMMON(...)                               \
  partition_alloc::internal::base::ScopedClearLastError            \
      reentrancy_guard_last_error_scope;                           \
  if (!base::internal::ReentrancyState::TryEnter()) [[unlikely]] { \
    return __VA_ARGS__;                                            \
  }                                                                \
  static_assert(true)

#endif  // !PA_BUILDFLAG(IS_WIN)

#define ALLOC_REENTRANCY_GUARD() \
  REENTRANCY_GUARD_COMMON();     \
  base::internal::ReentrancyState::Scope reentrancy_guard_scope

#define IGNORE_FREE_REENTRANCY_GUARD() \
  REENTRANCY_GUARD_COMMON(0);          \
  base::internal::ReentrancyState::ScopeAtFree reentrancy_guard_scope

#define FREE_REENTRANCY_GUARD() \
  REENTRANCY_GUARD_COMMON();    \
  base::internal::ReentrancyState::ScopeAtFree reentrancy_guard_scope

#else  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && (PA_BUILDFLAG(IS_POSIX) ||
       // PA_BUILDFLAG(IS_WIN))

// No-op
#define ALLOC_REENTRANCY_GUARD()
#define IGNORE_FREE_REENTRANCY_GUARD()
#define FREE_REENTRANCY_GUARD()

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && (PA_BUILDFLAG(IS_POSIX)
        // || PA_BUILDFLAG(IS_WIN))

#endif  // BASE_MEMORY_REENTRANCY_GUARD_H_
