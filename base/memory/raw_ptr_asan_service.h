// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_
#define BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_asan_allocator.h"
#include "base/memory/raw_ptr_asan_event.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/thread_annotations.h"
#include "base/types/strong_alias.h"

namespace base {

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace internal {
class AsanBackupRefPtrTest;
}  // namespace internal

struct RawPtrAsanServiceOptions {
  enum class EnableToggle : uint8_t {
    kDisabled,
    kEnabled,
  };
  static constexpr auto kDisabled = EnableToggle::kDisabled;
  static constexpr auto kEnabled = EnableToggle::kEnabled;

  EnableToggle enable_data_race_check = kDisabled;
  EnableToggle enable_free_after_quarantined_check = kDisabled;
};

#else

using EnableDereferenceCheck =
    base::StrongAlias<class EnableDereferenceCheckTag, bool>;
using EnableExtractionCheck =
    base::StrongAlias<class EnableExtractionCheckTag, bool>;
using EnableInstantiationCheck =
    base::StrongAlias<class EnableInstantiationCheckTag, bool>;
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

class BASE_EXPORT RawPtrAsanService {
 public:
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  // The `kSizeOfMapWithLockArray` should be enough large to avoid that
  // multiple threads requires the same lock.
  constexpr static size_t kSizeOfMapWithLockArray = 37u;

  using RefCountType = uint32_t;
  constexpr static size_t kMaxPtrCount =
      std::numeric_limits<RefCountType>::max();

  enum class QuarantineFlag {
    NotQuarantined = 0,
    Quarantined,
    EarlyAllocation,
  };

  struct AllocationMetadata {
    RefCountType count = 0u;
    QuarantineFlag quarantine_flag = QuarantineFlag::NotQuarantined;
    internal::RawPtrAsanThreadId alloc_thread_id;  // who allocated
    internal::RawPtrAsanThreadId free_thread_id;   // who quarantined
  };

  template <typename Key, typename T>
  class MapWithLock {
   public:
    using MapAllocator = internal::RawPtrAsanAllocator<std::pair<const Key, T>>;
    using Map = std::
        unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, MapAllocator>;
    using Lock = partition_alloc::internal::Lock;

    Lock& GetLock() { return lock_; }

    Map& GetMap() EXCLUSIVE_LOCKS_REQUIRED(GetLock()) { return map_; }

   private:
    Lock lock_;
    Map map_ GUARDED_BY(lock_);
  };
#else
  enum class ReportType {
    kDereference,
    kExtraction,
    kInstantiation,
  };

  struct PendingReport {
    ReportType type = ReportType::kDereference;
    uintptr_t allocation_base = 0;
    size_t allocation_size = 0;
  };
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  void Configure(bool enabled, RawPtrAsanServiceOptions options);
#else
  void Configure(EnableDereferenceCheck,
                 EnableExtractionCheck,
                 EnableInstantiationCheck);
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

  bool IsSupportedAllocation(const void*) const;

  bool IsEnabled() const { return mode_ == Mode::kEnabled; }

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  NO_SANITIZE("address")
  ALWAYS_INLINE bool is_data_race_check_enabled() const {
    return is_data_race_check_enabled_;
  }

  NO_SANITIZE("address")
  ALWAYS_INLINE bool is_free_after_quarantined_check_enabled() const {
    return is_free_after_quarantined_check_enabled_;
  }
#else
  NO_SANITIZE("address")
  ALWAYS_INLINE bool is_dereference_check_enabled() const {
    return is_dereference_check_enabled_;
  }

  NO_SANITIZE("address")
  ALWAYS_INLINE bool is_extraction_check_enabled() const {
    return is_extraction_check_enabled_;
  }

  NO_SANITIZE("address")
  ALWAYS_INLINE bool is_instantiation_check_enabled() const {
    return is_instantiation_check_enabled_;
  }
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

  NO_SANITIZE("address") ALWAYS_INLINE static RawPtrAsanService& GetInstance() {
    return instance_;
  }

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  uintptr_t GetAllocationStart(uintptr_t address) const;
  void AcquireInternal(uintptr_t address, bool is_copy = false) const;
  void ReleaseInternal(uintptr_t address) const;

  bool IsQuarantined(uintptr_t address) const;
  bool IsFreed(uintptr_t address) const;
  void CheckLogAndAbortOnError();

  internal::RawPtrAsanThreadId GetFreeThreadIdOfAllocation(
      uintptr_t allocation_address) const;
#else
  void WarnOnDanglingExtraction(const volatile void* ptr) const;
  void CrashOnDanglingInstantiation(const volatile void* ptr) const;

  static void SetPendingReport(ReportType type, const volatile void* ptr);
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

 private:
  enum class Mode {
    kUninitialized,
    kDisabled,
    kEnabled,
  };

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  friend class internal::AsanBackupRefPtrTest;

  using AllocationMetadataMap =
      RawPtrAsanService::MapWithLock<uintptr_t,
                                     RawPtrAsanService::AllocationMetadata>;

  static AllocationMetadataMap& GetAllocationMetadataMap(uintptr_t address);
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

  uint8_t* GetShadow(void* ptr) const;
  const uint8_t* GetShadow(const void* ptr) const;

  static void MallocHook(const volatile void*, size_t);
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  static int IgnoreFreeHook(const volatile void* ptr);
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  static void FreeHook(const volatile void*) {}
  static void ErrorReportCallback(const char* reason,
                                  bool* should_exit_cleanly,
                                  bool* should_abort);

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  static void ExitCallback(void*);

  bool CheckFaultAddress(uintptr_t fault_address = 0u, bool print_event = true);
  void ClearLogForTesting();  // IN-TEST

  void LogEvent(internal::RawPtrAsanEvent::Type event_type,
                uintptr_t event_address,
                size_t event_size) const;
  void Reset();
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

  Mode mode_ = Mode::kUninitialized;
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  bool is_data_race_check_enabled_ = false;
  bool is_free_after_quarantined_check_enabled_ = false;
#else
  bool is_dereference_check_enabled_ = false;
  bool is_extraction_check_enabled_ = false;
  bool is_instantiation_check_enabled_ = false;
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

  size_t shadow_offset_ = 0;

  static RawPtrAsanService instance_;  // Not a static local variable because
                                       // `GetInstance()` is used in hot paths.
};

}  // namespace base

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#endif  // BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_
