// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_service.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>
#include <stdarg.h>
#include <string.h>

#include <array>
#include <cstring>
#include <ranges>
#include <string_view>
#include <type_traits>

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/debug/asan_service.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_asan_bound_arg_tracker.h"
#include "base/memory/raw_ptr_asan_event.h"
#include "base/memory/raw_ptr_asan_hooks.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool/thread_group.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
#include "base/debug/leak_annotations.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "build/sanitizers/sanitizer_shared_hooks.h"  // nogncheck
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_lock.h"
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace base {

RawPtrAsanService RawPtrAsanService::instance_;

namespace {

// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_mapping.h#L154
constexpr size_t kShadowScale = 3;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_allocator.cpp#L143
constexpr size_t kChunkHeaderSize = 16;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L138
constexpr uint8_t kAsanHeapLeftRedzoneMagic = 0xfa;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L145
constexpr uint8_t kAsanUserPoisonedMemoryMagic = 0xf7;

}  // namespace

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace {
template <typename Key, typename T, size_t kNum>
class MapWithLockArray {
 public:
  NO_SANITIZE("address")
  RawPtrAsanService::MapWithLock<Key, T>& Get(const Key& key) {
    size_t index = GetMapIndex(key);
    return maps_[index];
  }

  NO_SANITIZE("address")
  RawPtrAsanService::MapWithLock<Key, T>& GetByIndex(size_t index) {
    return maps_[index];
  }

 private:
  NO_SANITIZE("address")
  static size_t GetMapIndex(const Key& key) {
    return std::hash<Key>{}(key) % kNum;
  }

  std::array<RawPtrAsanService::MapWithLock<Key, T>, kNum> maps_;
};

// To avoid the case where multiple threads are trying memory allocation /
// deallocation and are waiting until 1 metadata map is locked inside
// malloc or ignore free hook, we prepare multiple maps per allocation /
// deallocation address and reduce the number of lock waits.
using AllocationMetadataMapArray =
    MapWithLockArray<uintptr_t,
                     RawPtrAsanService::AllocationMetadata,
                     RawPtrAsanService::kSizeOfMapWithLockArray>;

AllocationMetadataMapArray& GetAllocationMetadataMapArray() {
  static base::NoDestructor<AllocationMetadataMapArray> s_map;
  return *s_map.get();
}

internal::RawPtrAsanEventLog& GetEventLog() {
  static base::NoDestructor<internal::RawPtrAsanEventLog> s_event_log;
  return *s_event_log.get();
}

}  // namespace

// static
RawPtrAsanService::AllocationMetadataMap&
RawPtrAsanService::GetAllocationMetadataMap(uintptr_t address) {
  return GetAllocationMetadataMapArray().Get(address);
}

NO_SANITIZE("address")
void RawPtrAsanService::LogEvent(internal::RawPtrAsanEvent::Type event_type,
                                 uintptr_t event_address,
                                 size_t event_size) const {
  internal::RawPtrAsanEvent event;
  event.type = event_type;
  event.thread_id = internal::GetCurrentRawPtrAsanThreadId();
  event.address = event_address;
  event.size = event_size;
  // TODO(crbug.com/447520906): we don't need to collect LogEvent().
  base::debug::CollectStackTrace(event.stack);
  GetEventLog().Add(std::move(event));
}

void RawPtrAsanService::CheckLogAndAbortOnError() {
  if (CheckFaultAddress(/*fault_address=*/0u, /*print_event=*/true)) {
    base::debug::AsanService::GetInstance()->Abort();
  }
}

NO_SANITIZE("address")
internal::RawPtrAsanThreadId RawPtrAsanService::GetFreeThreadIdOfAllocation(
    uintptr_t address) const {
  uintptr_t allocation_start_address = GetAllocationStart(address);
  auto& map =
      RawPtrAsanService::GetAllocationMetadataMap(allocation_start_address);
  internal::PartitionAutoLock lock(map.GetLock());
  auto it = map.GetMap().find(allocation_start_address);
  CHECK(it != map.GetMap().end());
  return it->second.free_thread_id;
}

void RawPtrAsanService::ClearLogForTesting() {
  GetEventLog().ClearForTesting();  // IN-TEST
}

NO_SANITIZE("address")
void RawPtrAsanService::Reset() {
  internal::ResetRawPtrHooks();
  build_sanitizers::UninstallSanitizerHooks();

  // We have to free all quarantined and early-allocated objects because
  // we use `ignore free hook` to block their deallocation. So the objects
  // look leaks. If we don't free, tons of leaks will be reported when run
  // with `detect_leaks=1`. Even if ignoring the leaked objects, LSan will
  // spend lots of time processing the ignored objects. It will cause
  // interactive_ui_tests failures.
  if (!debug::AsanService::GetInstance()->detect_leak()) {
    return;
  }
  for (size_t i = 0; i < RawPtrAsanService::kSizeOfMapWithLockArray; ++i) {
    auto& map = GetAllocationMetadataMapArray().GetByIndex(i);
    internal::PartitionAutoLock lock(map.GetLock());
    auto& map_internal = map.GetMap();
    for (auto it = map_internal.begin(); it != map_internal.end(); ++it) {
      if (it->second.quarantine_flag != QuarantineFlag::NotQuarantined) {
        free(reinterpret_cast<void*>(it->first));
      }
    }
    map_internal.clear();
  }
}

// static
NO_SANITIZE("address")
void RawPtrAsanService::ExitCallback(void*) {
  RawPtrAsanService& service = RawPtrAsanService::GetInstance();
  service.CheckLogAndAbortOnError();
  service.Reset();
}

#else

namespace {

// Intentionally use thread-local-storage here. Making this sequence-local
// doesn't prevent sharing of PendingReport contents between unrelated tasks, so
// we keep this at a lower-level and avoid introducing additional assumptions
// about Chrome's sequence model.
constinit thread_local RawPtrAsanService::PendingReport pending_report;

}  // namespace
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

// Mark the first eight bytes of every allocation's header as "user poisoned".
// This allows us to filter out allocations made before BRP-ASan is activated.
// The change shouldn't reduce the regular ASan coverage.

// static
NO_SANITIZE("address")
void RawPtrAsanService::MallocHook(const volatile void* ptr, size_t size) {
  uint8_t* header = UNSAFE_TODO(static_cast<uint8_t*>(const_cast<void*>(ptr)) -
                                kChunkHeaderSize);
  *RawPtrAsanService::GetInstance().GetShadow(header) =
      kAsanUserPoisonedMemoryMagic;

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  auto alloc_thread_id = internal::GetCurrentRawPtrAsanThreadId();
  {
    auto& map = GetAllocationMetadataMap(reinterpret_cast<uintptr_t>(ptr));
    internal::PartitionAutoLock lock(map.GetLock());
    [[maybe_unused]] auto result = map.GetMap().insert(
        {reinterpret_cast<uintptr_t>(ptr),
         {0u, QuarantineFlag::NotQuarantined, alloc_thread_id,
          internal::RawPtrAsanThreadId()}});
    PA_DCHECK(result.second);
  }
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}

NO_SANITIZE("address")
bool RawPtrAsanService::IsSupportedAllocation(
    const void* allocation_start) const {
  const uint8_t* header = UNSAFE_TODO(
      static_cast<const uint8_t*>(allocation_start) - kChunkHeaderSize);
  return *GetShadow(header) == kAsanUserPoisonedMemoryMagic;
}

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
// static
NO_SANITIZE("address")
int RawPtrAsanService::IgnoreFreeHook(const volatile void* ptr) {
  uint8_t* header = UNSAFE_TODO(static_cast<uint8_t*>(const_cast<void*>(ptr)) -
                                kChunkHeaderSize);

  auto& service = RawPtrAsanService::GetInstance();

  size_t size = __sanitizer_get_allocated_size_fast(ptr);
  UNSAFE_BUFFERS(base::span<uint8_t> region(
      reinterpret_cast<uint8_t*>(const_cast<void*>(ptr)), size));

  if (*service.GetShadow(header) != kAsanUserPoisonedMemoryMagic) {
    // If BRP is enabled at the same time of the first memory allocation,
    // (i.e. PartitionAlloc-Everywhere's default root is brp-enabled)
    // memory allocations before sanitizer hooks are installed will be
    // brp-refcounted but we cannot find the refcounts.
    // But if an early-allocated chunk was freed, split into
    // smaller chunks, and re-allocated after BrpAsan is enabled,
    // we cannot find that the split chunk is early-allocated.
    // (because each chunk has it own header and if a chunk is split into
    // multiple chunks, only the first chunk keeps its header. Others'
    // headers will be prepared at the time.
    // So, we should enable BrpAsan as early as possible. E.g.
    // TestSuite::TestSuite().
    {
      auto& map = GetAllocationMetadataMap(reinterpret_cast<uintptr_t>(ptr));
      internal::PartitionAutoLock guard(map.GetLock());
      [[maybe_unused]] auto result = map.GetMap().insert(
          {reinterpret_cast<uintptr_t>(ptr),
           {0u, QuarantineFlag::EarlyAllocation, internal::RawPtrAsanThreadId(),
            internal::RawPtrAsanThreadId()}});
      PA_DCHECK(result.second);
    }

    // The early allocation may have been already poisoned. If we always try
    // fill the region with the quarantine byte, we will see use-after-poison.
    // TODO(crbug.com/447520906): investigate why the memory region has
    // been already poisoned before the region is freed.
    if (!__asan_region_is_poisoned(const_cast<void*>(ptr), size)) {
      std::ranges::fill(region, partition_alloc::internal::kQuarantinedByte);
    }

    // When accessing any quarantined memory, cause `use-after-poison`.
    __asan_poison_memory_region(ptr, size);

    ANNOTATE_LEAKING_OBJECT_PTR(const_cast<const void*>(ptr));
    return 1;
  }

  internal::RawPtrAsanThreadId current_thread_id =
      internal::GetCurrentRawPtrAsanThreadId();
  {
    auto& map = GetAllocationMetadataMap(reinterpret_cast<uintptr_t>(ptr));
    internal::PartitionAutoLock guard(map.GetLock());
    auto it = map.GetMap().find(reinterpret_cast<uintptr_t>(ptr));
    PA_DCHECK(it != map.GetMap().end());
    if (it == map.GetMap().end()) {
      return 0;
    }
    if (it->second.count == 0) {
      map.GetMap().erase(it);
      return 0;
    }
    it->second.quarantine_flag = QuarantineFlag::Quarantined;
    it->second.free_thread_id = current_thread_id;
  }

  std::ranges::fill(region, partition_alloc::internal::kQuarantinedByte);

  // When accessing any quarantined memory, cause `use-after-poison`.
  __asan_poison_memory_region(ptr, size);

  // This allocation is being quarantined, so tell the ASan allocator not to
  // release it yet.
  ANNOTATE_LEAKING_OBJECT_PTR(const_cast<const void*>(ptr));
  return 1;
}

#endif

NO_SANITIZE("address")
void RawPtrAsanService::Configure(
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
    bool enabled,
    RawPtrAsanServiceOptions options
#else
    EnableDereferenceCheck enable_dereference_check,
    EnableExtractionCheck enable_extraction_check,
    EnableInstantiationCheck enable_instantiation_check
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
) {
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  Mode new_mode = enabled ? Mode::kEnabled : Mode::kDisabled;
  CHECK(mode_ == Mode::kUninitialized ||
        mode_ == Mode::kEnabled && new_mode == Mode::kEnabled);

  if (mode_ == Mode::kEnabled && new_mode == Mode::kEnabled) {
    is_data_race_check_enabled_ =
        options.enable_data_race_check == RawPtrAsanServiceOptions::kEnabled;
    is_free_after_quarantined_check_enabled_ =
        options.enable_free_after_quarantined_check ==
        RawPtrAsanServiceOptions::kEnabled;
    return;
  }
#else
  CHECK_EQ(mode_, Mode::kUninitialized);

  Mode new_mode = enable_dereference_check || enable_extraction_check ||
                          enable_instantiation_check
                      ? Mode::kEnabled
                      : Mode::kDisabled;
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  if (new_mode == Mode::kEnabled) {
    // The constants we use aren't directly exposed by the API, so
    // validate them at runtime as carefully as possible.
    size_t shadow_scale;
    __asan_get_shadow_mapping(&shadow_scale, &shadow_offset_);
    CHECK_EQ(shadow_scale, kShadowScale);

    uint8_t* dummy_alloc = new uint8_t;
    CHECK_EQ(*GetShadow(UNSAFE_TODO(dummy_alloc - kChunkHeaderSize)),
             kAsanHeapLeftRedzoneMagic);

    __asan_poison_memory_region(dummy_alloc, 1);
    CHECK_EQ(*GetShadow(dummy_alloc), kAsanUserPoisonedMemoryMagic);
    delete dummy_alloc;

    debug::AsanService* asan_service = debug::AsanService::GetInstance();
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
    // There is no way to see `asan::flags()->halt_on_error`.
    asan_service->AddErrorCallback(ErrorReportCallback);
    AtExitManager::RegisterCallback(ExitCallback, nullptr);

    is_data_race_check_enabled_ =
        options.enable_data_race_check == RawPtrAsanServiceOptions::kEnabled;
    is_free_after_quarantined_check_enabled_ =
        options.enable_free_after_quarantined_check ==
        RawPtrAsanServiceOptions::kEnabled;

    internal::InstallRawPtrHooks(internal::GetRawPtrAsanHooks());

    build_sanitizers::InstallSanitizerHooks(RawPtrAsanService::MallocHook,
                                            nullptr,
                                            RawPtrAsanService::IgnoreFreeHook);
#else
    __sanitizer_install_malloc_and_free_hooks(MallocHook, FreeHook);
    asan_service->AddErrorCallback(ErrorReportCallback);
    internal::InstallRawPtrHooks(internal::GetRawPtrAsanHooks());

    is_dereference_check_enabled_ = !!enable_dereference_check;
    is_extraction_check_enabled_ = !!enable_extraction_check;
    is_instantiation_check_enabled_ = !!enable_instantiation_check;
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
  }

  mode_ = new_mode;
}

NO_SANITIZE("address")
uint8_t* RawPtrAsanService::GetShadow(void* ptr) const {
  return reinterpret_cast<uint8_t*>(
      (reinterpret_cast<uintptr_t>(ptr) >> kShadowScale) + shadow_offset_);
}

NO_SANITIZE("address")
const uint8_t* RawPtrAsanService::GetShadow(const void* ptr) const {
  return reinterpret_cast<uint8_t*>(
      (reinterpret_cast<uintptr_t>(ptr) >> kShadowScale) + shadow_offset_);
}

#if !PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
// static
void RawPtrAsanService::SetPendingReport(ReportType type,
                                         const volatile void* ptr) {
  // The actual ASan crash may occur at an offset from the pointer passed
  // here, so track the whole region.
  void* region_base;
  size_t region_size;
  __asan_locate_address(const_cast<void*>(ptr), nullptr, 0, &region_base,
                        &region_size);

  pending_report = {type, reinterpret_cast<uintptr_t>(region_base),
                    region_size};
}
#endif  // !PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

namespace {
enum class ProtectionStatus {
  kUnknown,
  kNotProtected,
  kManualAnalysisRequired,
  kProtected,
};

NO_SANITIZE("address")
const char* ProtectionStatusToString(ProtectionStatus status) {
  switch (status) {
    case ProtectionStatus::kUnknown:
      return "UNKNOWN";
    case ProtectionStatus::kNotProtected:
      return "NOT PROTECTED";
    case ProtectionStatus::kManualAnalysisRequired:
      return "MANUAL ANALYSIS REQUIRED";
    case ProtectionStatus::kProtected:
      return "PROTECTED";
  }
}

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
struct CrashInfo {
  ProtectionStatus protection_status;
  const char* crash_details;
  const char* protection_details;
};

NO_SANITIZE("address")
void SetCrashInfo(CrashInfo& crash_info,
                  ProtectionStatus protection_status,
                  const char* crash_details,
                  const char* protection_details) {
  if (crash_info.protection_status != ProtectionStatus::kNotProtected) {
    crash_info.protection_status = protection_status;
    crash_info.crash_details = crash_details;
    crash_info.protection_details = protection_details;
  }
}

NO_SANITIZE("address")
bool CheckLog(uintptr_t fault_address, CrashInfo& crash_info) {
  bool fault_address_matched = false;

  internal::PartitionAutoLock event_log_lock(GetEventLog().GetLock());
  internal::RawPtrAsanVector<internal::RawPtrAsanEvent>& events =
      GetEventLog().events();

  for (size_t i = 0; i < events.size(); ++i) {
    // If we find a pointer laundering event, we do not continue
    if (events[i].type == internal::RawPtrAsanEvent::Type::kFreeAssignment) {
      SetCrashInfo(
          crash_info, ProtectionStatus::kNotProtected,
          "A pointer to a \"freed\" non-quarantined allocation was assigned "
          "to a raw_ptr<T>. This results in bypassing the MiraclePtr "
          "protection.",
          "This crash is exploitable with MiraclePtr.");
      break;
    }

    if (events[i].type != internal::RawPtrAsanEvent::Type::kQuarantineEntry) {
      continue;
    }

    // We're now at a quarantine-entry event. We want to scan through the rest
    // of the events for this allocation, and determine whether the accesses
    // to this particular quarantined allocation were safe.
    for (size_t j = i + 1; j < events.size(); ++j) {
      if (!events[i].IsSameAllocation(events[j])) {
        continue;
      }

      if (events[i].thread_id != events[j].thread_id) {
        SetCrashInfo(
            crash_info, ProtectionStatus::kNotProtected,
            "A quarantined allocation was accessed from a thread which "
            "doesn't match the thread which called \"free\" on the "
            "allocation. This is likely to have been caused by a race "
            "condition that is mislabeled as a use-after-free.",
            "This crash is probably still exploitable with MiraclePtr.");
        break;
      }

      if (events[j].type == internal::RawPtrAsanEvent::Type::kQuarantineRead ||
          events[j].type == internal::RawPtrAsanEvent::Type::kQuarantineWrite) {
        if (events[j].type ==
                internal::RawPtrAsanEvent::Type::kQuarantineRead &&
            fault_address && fault_address == events[j].fault_address) {
          fault_address_matched = true;
        }
        SetCrashInfo(
            crash_info, ProtectionStatus::kProtected,
            "This crash is an access to an allocation quarantined by "
            "MiraclePtr, which did not result in a memory safety error that "
            "would be observed in production builds.",
            "This crash is not exploitable with MiraclePtr.");
      } else if (events[j].type ==
                 internal::RawPtrAsanEvent::Type::kQuarantineAssignment) {
        SetCrashInfo(
            crash_info, ProtectionStatus::kProtected,
            "This crash is an assignment to a raw_ptr<T> of a pointer to a "
            "dangling (quarantined) allocation. This is a bug, but it did "
            "not result in a memory safety error that would be observed in "
            "production builds.",
            "This crash is not exploitable with MiraclePtr.");
      }
    }
  }
  return fault_address_matched;
}
#else
// ASan doesn't have an API to get the current thread's identifier.
// We have to create a dummy allocation to determine it.
int GetCurrentThreadId() {
  int* dummy = new int;
  int id = -1;
  __asan_get_alloc_stack(dummy, nullptr, 0, &id);
  delete dummy;
  return id;
}
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
}  // namespace

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
// static
NO_SANITIZE("address")
void RawPtrAsanService::ErrorReportCallback(const char* reason,
                                            bool* should_exit_cleanly,
                                            bool* should_abort) {
  RawPtrAsanService& service = RawPtrAsanService::GetInstance();
  const char* description = __asan_get_report_description();
  void* fault_ptr = __asan_get_report_address();
  uintptr_t fault_address = reinterpret_cast<uintptr_t>(fault_ptr);

  // All accesses to BRP-quarantined memory will be "use-after-poison".
  bool is_use_after_poison =
      description && std::string_view(description) == "use-after-poison";
  bool abort = true;
  if (!debug::AsanService::GetInstance()->halt_on_error()) {
    if (is_use_after_poison) {
      if (service.IsQuarantined(fault_address)) {
        if (__asan_get_report_access_type()) {
          service.LogEvent(internal::RawPtrAsanEvent::Type::kQuarantineWrite,
                           fault_address, __asan_get_report_access_size());
        } else {
          service.LogEvent(internal::RawPtrAsanEvent::Type::kQuarantineRead,
                           fault_address, __asan_get_report_access_size());
        }

        // We want to continue execution, rather than aborting at the first
        // access to quarantined memory.
        abort = false;
      }
    }
  }

  // Even if we continue, we have to report the error as protected.
  // Because if the process crashes because of (e.g.) sigsegv,
  // the process may not invoke ExitCallback (e.g. libFuzzer).
  // In this case, the error will be reported without "PROTECTED".
  std::ignore = service.CheckFaultAddress(fault_address, /*print_event=*/false);
  *should_abort = abort;
}

NO_SANITIZE("address")
bool RawPtrAsanService::CheckFaultAddress(uintptr_t fault_address,
                                          bool print_event) {
  CrashInfo crash_info = {
      ProtectionStatus::kUnknown,
      "This should not happen. If you have a testcase that produces this "
      "message, please contact the BRP-ASan developers for assistance.",
      "This is likely a bug in MiraclePtr tooling.",
  };

  bool fault_address_matched = CheckLog(fault_address, crash_info);
  if (fault_address) {
    if (!fault_address_matched) {
      // `fault_address` was not found in the log. But this doesn't mean `not
      // protected`. If the memory region is quarantined, free() was invoked to
      // deallocate the memory region but miracle pointer blocked the
      // deallocation and instead quarantined. So accessing the `fault_address`
      // was protected.
      if (IsQuarantined(fault_address)) {
        fault_address_matched = true;
      }
    }
    if (fault_address_matched) {
      SetCrashInfo(
          crash_info, ProtectionStatus::kProtected,
          "This crash is an access through a zapped pointer, resulting from a "
          "read from an allocation quarantined by MiraclePtr. This will result "
          "in a safe crash in production builds.",
          "This crash is not exploitable with MiraclePtr.");
    } else {
      SetCrashInfo(
          crash_info, ProtectionStatus::kNotProtected,
          "This crash is not protected by MiraclePtr at all. Either it is an "
          "error that is not protected, such as an out-of-bounds access, or it "
          "is a use-after-free of an object that has not been quarantined by "
          "MiraclePtr, either because it is an unsupported allocation or "
          "because "
          "there were no live raw_ptr references when the allocation was "
          "freed.",
          "This crash is still exploitable with MiraclePtr.");
    }
  }

  if (fault_address ||
      crash_info.protection_status != ProtectionStatus::kUnknown) {
    // A report-worthy event has occurred, so we want to print the full event
    // log and the protection information.

    if (print_event) {
      GetEventLog().Print(/*print_stack=*/true);
    }

    debug::AsanService::GetInstance()->Log(
        "\nMiraclePtr Status: %s\n%s\n%s\n"
        "Refer to "
        "https://chromium.googlesource.com/chromium/src/+/main/base/memory/"
        "raw_ptr.md for details.",
        ProtectionStatusToString(crash_info.protection_status),
        crash_info.crash_details, crash_info.protection_details);

    return true;
  }

  return false;
}

#else
// static
void RawPtrAsanService::ErrorReportCallback(const char* reason,
                                            bool* should_exit_cleanly,
                                            bool* should_abort) {
  if (UNSAFE_TODO(strcmp(__asan_get_report_description(),
                         "heap-use-after-free")) != 0) {
    return;
  }

  struct {
    ProtectionStatus protection_status;
    const char* crash_details;
    const char* protection_details;
  } crash_info;

  uintptr_t ptr = reinterpret_cast<uintptr_t>(__asan_get_report_address());
  uintptr_t bound_arg_ptr = RawPtrAsanBoundArgTracker::GetProtectedArgPtr(ptr);
  if (pending_report.allocation_base <= ptr &&
      ptr < pending_report.allocation_base + pending_report.allocation_size) {
    bool is_supported_allocation =
        RawPtrAsanService::GetInstance().IsSupportedAllocation(
            reinterpret_cast<void*>(pending_report.allocation_base));
    switch (pending_report.type) {
      case ReportType::kDereference: {
        if (is_supported_allocation) {
          crash_info = {ProtectionStatus::kProtected,
                        "This crash occurred while a raw_ptr<T> object "
                        "containing a dangling pointer was being dereferenced.",
                        "MiraclePtr is expected to make this crash "
                        "non-exploitable once fully enabled."};
        } else {
          crash_info = {ProtectionStatus::kNotProtected,
                        "This crash occurred while accessing a region that was "
                        "allocated before MiraclePtr was activated.",
                        "This crash is still exploitable with MiraclePtr."};
        }
        break;
      }
      case ReportType::kExtraction: {
        if (is_supported_allocation && bound_arg_ptr) {
          crash_info = {
              ProtectionStatus::kProtected,
              "This crash occurred inside a callback where a raw_ptr<T> "
              "pointing to the same region was bound to one of the arguments.",
              "MiraclePtr is expected to make this crash non-exploitable once "
              "fully enabled."};
        } else if (is_supported_allocation) {
          crash_info = {
              ProtectionStatus::kManualAnalysisRequired,
              "A pointer to the same region was extracted from a raw_ptr<T> "
              "object prior to this crash.",
              "To determine the protection status, enable extraction warnings "
              "and check whether the raw_ptr<T> object can be destroyed or "
              "overwritten between the extraction and use."};
        } else {
          crash_info = {ProtectionStatus::kNotProtected,
                        "This crash occurred while accessing a region that was "
                        "allocated before MiraclePtr was activated.",
                        "This crash is still exploitable with MiraclePtr."};
        }
        break;
      }
      case ReportType::kInstantiation: {
        crash_info = {ProtectionStatus::kNotProtected,
                      "A pointer to an already freed region was assigned to a "
                      "raw_ptr<T> object, which may lead to memory corruption.",
                      "This crash is still exploitable with MiraclePtr."};
      }
    }
  } else if (bound_arg_ptr) {
    // Note - this branch comes second to avoid hiding invalid instantiations,
    // as we still consider it to be an error to instantiate a raw_ptr<T> from
    // an invalid T* even if that T* is guaranteed to be quarantined.
    bool is_supported_allocation =
        RawPtrAsanService::GetInstance().IsSupportedAllocation(
            reinterpret_cast<void*>(bound_arg_ptr));
    if (is_supported_allocation) {
      crash_info = {
          ProtectionStatus::kProtected,
          "This crash occurred inside a callback where a raw_ptr<T> pointing "
          "to the same region was bound to one of the arguments.",
          "MiraclePtr is expected to make this crash non-exploitable once "
          "fully enabled."};
    } else {
      crash_info = {ProtectionStatus::kNotProtected,
                    "This crash occurred while accessing a region that was "
                    "allocated before MiraclePtr was activated.",
                    "This crash is still exploitable with MiraclePtr."};
    }
  } else {
    crash_info = {
        ProtectionStatus::kNotProtected,
        "No raw_ptr<T> access to this region was detected prior to this crash.",
        "This crash is still exploitable with MiraclePtr."};
  }

  // The race condition check below may override the protection status.
  if (crash_info.protection_status != ProtectionStatus::kNotProtected) {
    int free_thread_id = -1;
    __asan_get_free_stack(reinterpret_cast<void*>(ptr), nullptr, 0,
                          &free_thread_id);
    if (free_thread_id != GetCurrentThreadId()) {
      crash_info.protection_status = ProtectionStatus::kManualAnalysisRequired;
      crash_info.protection_details =
          "The \"use\" and \"free\" threads don't match. This crash is likely "
          "to have been caused by a race condition that is mislabeled as a "
          "use-after-free. Make sure that the \"free\" is sequenced after the "
          "\"use\" (e.g. both are on the same sequence, or the \"free\" is in "
          "a task posted after the \"use\"). Otherwise, the crash is still "
          "exploitable with MiraclePtr.";
    } else if (internal::ThreadGroup::CurrentThreadHasGroup()) {
      // We need to be especially careful with ThreadPool threads. Otherwise,
      // we might miss false-positives where the "use" and "free" happen on
      // different sequences but the same thread by chance.
      crash_info.protection_status = ProtectionStatus::kManualAnalysisRequired;
      crash_info.protection_details =
          "This crash occurred in the thread pool. The sequence which invoked "
          "the \"free\" is unknown, so the crash may have been caused by a "
          "race condition that is mislabeled as a use-after-free. Make sure "
          "that the \"free\" is sequenced after the \"use\" (e.g. both are on "
          "the same sequence, or the \"free\" is in a task posted after the "
          "\"use\"). Otherwise, the crash is still exploitable with "
          "MiraclePtr.";
    }
  }

  debug::AsanService::GetInstance()->Log(
      "\nMiraclePtr Status: %s\n%s\n%s\n"
      "Refer to "
      "https://chromium.googlesource.com/chromium/src/+/main/base/memory/"
      "raw_ptr.md for details.",
      ProtectionStatusToString(crash_info.protection_status),
      crash_info.crash_details, crash_info.protection_details);
}

namespace {
enum class MessageLevel {
  kWarning,
  kError,
};

const char* LevelToString(MessageLevel level) {
  switch (level) {
    case MessageLevel::kWarning:
      return "WARNING";
    case MessageLevel::kError:
      return "ERROR";
  }
}

// Prints AddressSanitizer-like custom error messages.
void Log(MessageLevel level,
         uintptr_t address,
         const char* type,
         const char* description) {
#if __has_builtin(__builtin_extract_return_addr) && \
    __has_builtin(__builtin_return_address)
  void* pc = __builtin_extract_return_addr(__builtin_return_address(0));
#else
  void* pc = nullptr;
#endif

#if __has_builtin(__builtin_frame_address)
  void* bp = __builtin_frame_address(0);
#else
  void* bp = nullptr;
#endif

  void* local_stack;
  void* sp = &local_stack;

  debug::AsanService::GetInstance()->Log(
      "=================================================================\n"
      "==%d==%s: MiraclePtr: %s on address %p at pc %p bp %p sp %p",
      Process::Current().Pid(), LevelToString(level), type, address, pc, bp,
      sp);
  __sanitizer_print_stack_trace();
  __asan_describe_address(reinterpret_cast<void*>(address));
  debug::AsanService::GetInstance()->Log(
      "%s\n"
      "=================================================================",
      description);
}
}  // namespace

void RawPtrAsanService::WarnOnDanglingExtraction(
    const volatile void* ptr) const {
  Log(MessageLevel::kWarning, reinterpret_cast<uintptr_t>(ptr),
      "dangling-pointer-extraction",
      "A regular ASan report will follow if the extracted pointer is "
      "dereferenced later.\n"
      "Otherwise, it is still likely a bug to rely on the address of an "
      "already freed allocation.\n"
      "Refer to "
      "https://chromium.googlesource.com/chromium/src/+/main/base/memory/"
      "raw_ptr.md for details.");
}

void RawPtrAsanService::CrashOnDanglingInstantiation(
    const volatile void* ptr) const {
  Log(MessageLevel::kError, reinterpret_cast<uintptr_t>(ptr),
      "dangling-pointer-instantiation",
      "This crash occurred due to an attempt to assign a dangling pointer to a "
      "raw_ptr<T> variable, which might lead to use-after-free.\n"
      "Note that this report might be a false positive if at the moment of the "
      "crash another raw_ptr<T> is guaranteed to keep the allocation alive.\n"
      "Refer to "
      "https://chromium.googlesource.com/chromium/src/+/main/base/memory/"
      "raw_ptr.md for details.");
  base::ImmediateCrash();
}

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

}  // namespace base

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
