// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <windows.h>  // Must be in front of other Windows header files.
#include <winternl.h>

#include <psapi.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/byte_count.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {
namespace {

// ntstatus.h conflicts with windows.h so define this locally.
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

// Definition of this struct is taken from the book:
// Windows NT/200, Native API reference, Gary Nebbett
struct SYSTEM_PERFORMANCE_INFORMATION {
  // Total idle time of all processes in the system (units of 100 ns).
  LARGE_INTEGER IdleTime;
  // Number of bytes read (by all call to ZwReadFile).
  LARGE_INTEGER ReadTransferCount;
  // Number of bytes written (by all call to ZwWriteFile).
  LARGE_INTEGER WriteTransferCount;
  // Number of bytes transferred (e.g. DeviceIoControlFile)
  LARGE_INTEGER OtherTransferCount;
  // The amount of read operations.
  ULONG ReadOperationCount;
  // The amount of write operations.
  ULONG WriteOperationCount;
  // The amount of other operations.
  ULONG OtherOperationCount;
  // The number of pages of physical memory available to processes running on
  // the system.
  ULONG AvailablePages;
  ULONG TotalCommittedPages;
  ULONG TotalCommitLimit;
  ULONG PeakCommitment;
  ULONG PageFaults;
  ULONG WriteCopyFaults;
  ULONG TransitionFaults;
  ULONG CacheTransitionFaults;
  ULONG DemandZeroFaults;
  // The number of pages read from disk to resolve page faults.
  ULONG PagesRead;
  // The number of read operations initiated to resolve page faults.
  ULONG PageReadIos;
  ULONG CacheReads;
  ULONG CacheIos;
  // The number of pages written to the system's pagefiles.
  ULONG PagefilePagesWritten;
  // The number of write operations performed on the system's pagefiles.
  ULONG PagefilePageWriteIos;
  ULONG MappedFilePagesWritten;
  ULONG MappedFilePageWriteIos;
  ULONG PagedPoolUsage;
  ULONG NonPagedPoolUsage;
  ULONG PagedPoolAllocs;
  ULONG PagedPoolFrees;
  ULONG NonPagedPoolAllocs;
  ULONG NonPagedPoolFrees;
  ULONG TotalFreeSystemPtes;
  ULONG SystemCodePage;
  ULONG TotalSystemDriverPages;
  ULONG TotalSystemCodePages;
  ULONG SmallNonPagedLookasideListAllocateHits;
  ULONG SmallPagedLookasideListAllocateHits;
  ULONG Reserved3;
  ULONG MmSystemCachePage;
  ULONG PagedPoolPage;
  ULONG SystemDriverPage;
  ULONG FastReadNoWait;
  ULONG FastReadWait;
  ULONG FastReadResourceMiss;
  ULONG FastReadNotPossible;
  ULONG FastMdlReadNoWait;
  ULONG FastMdlReadWait;
  ULONG FastMdlReadResourceMiss;
  ULONG FastMdlReadNotPossible;
  ULONG MapDataNoWait;
  ULONG MapDataWait;
  ULONG MapDataNoWaitMiss;
  ULONG MapDataWaitMiss;
  ULONG PinMappedDataCount;
  ULONG PinReadNoWait;
  ULONG PinReadWait;
  ULONG PinReadNoWaitMiss;
  ULONG PinReadWaitMiss;
  ULONG CopyReadNoWait;
  ULONG CopyReadWait;
  ULONG CopyReadNoWaitMiss;
  ULONG CopyReadWaitMiss;
  ULONG MdlReadNoWait;
  ULONG MdlReadWait;
  ULONG MdlReadNoWaitMiss;
  ULONG MdlReadWaitMiss;
  ULONG ReadAheadIos;
  ULONG LazyWriteIos;
  ULONG LazyWritePages;
  ULONG DataFlushes;
  ULONG DataPages;
  ULONG ContextSwitches;
  ULONG FirstLevelTbFills;
  ULONG SecondLevelTbFills;
  ULONG SystemCalls;
};

base::expected<TimeDelta, ProcessCPUUsageError> GetImpreciseCumulativeCPUUsage(
    const win::ScopedHandle& process) {
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  if (!process.is_valid()) {
    return base::unexpected(ProcessCPUUsageError::kSystemError);
  }

  if (!GetProcessTimes(process.get(), &creation_time, &exit_time, &kernel_time,
                       &user_time)) {
    // This should never fail when the handle is valid.
    NOTREACHED();
  }

  return base::ok(TimeDelta::FromFileTime(kernel_time) +
                  TimeDelta::FromFileTime(user_time));
}

}  // namespace

size_t GetMaxFds() {
  // Windows is only limited by the amount of physical memory.
  return std::numeric_limits<size_t>::max();
}

size_t GetHandleLimit() {
  // Rounded down from value reported here:
  // http://blogs.technet.com/b/markrussinovich/archive/2009/09/29/3283844.aspx
  return static_cast<size_t>(1 << 23);
}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

base::expected<ProcessMemoryInfo, ProcessUsageError>
ProcessMetrics::GetMemoryInfo() const {
  if (!process_.is_valid()) {
    return base::unexpected(ProcessUsageError::kProcessNotFound);
  }
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (!::GetProcessMemoryInfo(process_.get(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                              sizeof(pmc))) {
    return base::unexpected(ProcessUsageError::kSystemError);
  }
  ProcessMemoryInfo counters;
  counters.private_bytes = pmc.PrivateUsage;
  counters.resident_set_bytes = pmc.WorkingSetSize;
  return counters;
}

base::expected<TimeDelta, ProcessCPUUsageError>
ProcessMetrics::GetCumulativeCPUUsage() {
  TRACE_EVENT("base", "GetCumulativeCPUUsage");
#if defined(ARCH_CPU_ARM64)
  // Precise CPU usage is not available on Arm CPUs because they don't support
  // constant rate TSC.
  return GetImpreciseCumulativeCPUUsage(process_);
#else   // !defined(ARCH_CPU_ARM64)
  if (!time_internal::HasConstantRateTSC()) {
    return GetImpreciseCumulativeCPUUsage(process_);
  }

  const double tsc_ticks_per_second = time_internal::TSCTicksPerSecond();
  if (tsc_ticks_per_second == 0) {
    // TSC is only initialized once TSCTicksPerSecond() is called twice 50 ms
    // apart on the same thread to get a baseline. In unit tests, it is frequent
    // for the initialization not to be complete. In production, it can also
    // theoretically happen.
    return GetImpreciseCumulativeCPUUsage(process_);
  }

  if (!process_.is_valid()) {
    return base::unexpected(ProcessCPUUsageError::kProcessNotFound);
  }

  ULONG64 process_cycle_time = 0;
  if (!QueryProcessCycleTime(process_.get(), &process_cycle_time)) {
    // This should never fail when the handle is valid.
    NOTREACHED();
  }

  const double process_time_seconds = process_cycle_time / tsc_ticks_per_second;
  return base::ok(Seconds(process_time_seconds));
#endif  // !defined(ARCH_CPU_ARM64)
}

ProcessMetrics::ProcessMetrics(ProcessHandle process) {
  if (process == kNullProcessHandle) {
    // Don't try to duplicate an invalid handle. However, INVALID_HANDLE_VALUE
    // is also the pseudo-handle returned by ::GetCurrentProcess(), so DO try
    // to duplicate that.
    return;
  }
  HANDLE duplicate_handle = INVALID_HANDLE_VALUE;
  BOOL result = ::DuplicateHandle(::GetCurrentProcess(), process,
                                  ::GetCurrentProcess(), &duplicate_handle,
                                  PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0);
  if (!result) {
    // Even with PROCESS_QUERY_LIMITED_INFORMATION, DuplicateHandle can fail
    // with ERROR_ACCESS_DENIED. And it's always possible to run out of handles.
    const DWORD last_error = ::GetLastError();
    CHECK(last_error == ERROR_ACCESS_DENIED ||
          last_error == ERROR_NO_SYSTEM_RESOURCES);
    return;
  }

  process_.Set(duplicate_handle);
}

size_t GetSystemCommitCharge() {
  PERFORMANCE_INFORMATION info;
  if (!::GetPerformanceInfo(&info, sizeof(info))) {
    DLOG(ERROR) << "Failed to fetch internal performance info.";
    return 0;
  }
  return (info.CommitTotal * info.PageSize) / 1024;
}

// This function uses the following mapping between MEMORYSTATUSEX and
// SystemMemoryInfo:
//   ullTotalPhys ==> total
//   ullAvailPhys ==> avail_phys
//   ullTotalPageFile ==> swap_total
//   ullAvailPageFile ==> swap_free
bool GetSystemMemoryInfo(SystemMemoryInfo* meminfo) {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status)) {
    return false;
  }

  meminfo->total = ByteCount::FromUnsigned(mem_status.ullTotalPhys);
  meminfo->avail_phys = ByteCount::FromUnsigned(mem_status.ullAvailPhys);
  meminfo->swap_total = ByteCount::FromUnsigned(mem_status.ullTotalPageFile);
  meminfo->swap_free = ByteCount::FromUnsigned(mem_status.ullAvailPageFile);

  return true;
}

size_t ProcessMetrics::GetMallocUsage() {
  // Unsupported as getting malloc usage on Windows requires iterating through
  // the heap which is slow and crashes.
  return 0;
}

SystemPerformanceInfo::SystemPerformanceInfo() = default;
SystemPerformanceInfo::SystemPerformanceInfo(
    const SystemPerformanceInfo& other) = default;
SystemPerformanceInfo& SystemPerformanceInfo::operator=(
    const SystemPerformanceInfo& other) = default;

// Retrieves performance counters from the operating system.
// Fills in the provided |info| structure. Returns true on success.
BASE_EXPORT bool GetSystemPerformanceInfo(SystemPerformanceInfo* info) {
  SYSTEM_PERFORMANCE_INFORMATION counters = {};
  {
    // The call to NtQuerySystemInformation might block on a lock.
    base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                  BlockingType::MAY_BLOCK);
    if (::NtQuerySystemInformation(::SystemPerformanceInformation, &counters,
                                   sizeof(SYSTEM_PERFORMANCE_INFORMATION),
                                   nullptr) != STATUS_SUCCESS) {
      return false;
    }
  }

  info->idle_time = static_cast<uint64_t>(counters.IdleTime.QuadPart);
  info->read_transfer_count =
      static_cast<uint64_t>(counters.ReadTransferCount.QuadPart);
  info->write_transfer_count =
      static_cast<uint64_t>(counters.WriteTransferCount.QuadPart);
  info->other_transfer_count =
      static_cast<uint64_t>(counters.OtherTransferCount.QuadPart);
  info->read_operation_count = counters.ReadOperationCount;
  info->write_operation_count = counters.WriteOperationCount;
  info->other_operation_count = counters.OtherOperationCount;
  info->pagefile_pages_written = counters.PagefilePagesWritten;
  info->pagefile_pages_write_ios = counters.PagefilePageWriteIos;
  info->available_pages = counters.AvailablePages;
  info->pages_read = counters.PagesRead;
  info->page_read_ios = counters.PageReadIos;

  return true;
}

ByteCount SystemMemoryInfo::GetAvailablePhysicalMemory() const {
  // Use ullAvailPhys from MEMORYSTATUSEX, which represents physical memory
  // available without paging.
  return avail_phys;
}

}  // namespace base
