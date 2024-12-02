// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <AvailabilityMacros.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>

#include <optional>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <libproc.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#else
#include <mach/vm_region.h>
#if BUILDFLAG(USE_BLINK)
#include "base/ios/sim_header_shims.h"
#endif  // BUILDFLAG(USE_BLINK)
#endif

namespace base {

#define TIME_VALUE_TO_TIMEVAL(a, r)   \
  do {                                \
    (r)->tv_sec = (a)->seconds;       \
    (r)->tv_usec = (a)->microseconds; \
  } while (0)

namespace {

base::expected<task_basic_info_64, ProcessCPUUsageError> GetTaskInfo(
    mach_port_t task) {
  if (task == MACH_PORT_NULL) {
    return base::unexpected(ProcessCPUUsageError::kProcessNotFound);
  }
  task_basic_info_64 task_info_data{};
  mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr =
      task_info(task, TASK_BASIC_INFO_64,
                reinterpret_cast<task_info_t>(&task_info_data), &count);
  // Most likely cause for failure: |task| is a zombie.
  if (kr != KERN_SUCCESS) {
    return base::unexpected(ProcessCPUUsageError::kSystemError);
  }
  return base::ok(task_info_data);
}

MachVMRegionResult ParseOutputFromMachVMRegion(kern_return_t kr) {
  if (kr == KERN_INVALID_ADDRESS) {
    // We're at the end of the address space.
    return MachVMRegionResult::Finished;
  } else if (kr != KERN_SUCCESS) {
    return MachVMRegionResult::Error;
  }
  return MachVMRegionResult::Success;
}

bool GetPowerInfo(mach_port_t task, task_power_info* power_info_data) {
  if (task == MACH_PORT_NULL) {
    return false;
  }

  mach_msg_type_number_t power_info_count = TASK_POWER_INFO_COUNT;
  kern_return_t kr = task_info(task, TASK_POWER_INFO,
                               reinterpret_cast<task_info_t>(power_info_data),
                               &power_info_count);
  // Most likely cause for failure: |task| is a zombie.
  return kr == KERN_SUCCESS;
}

}  // namespace

// Implementations of ProcessMetrics class shared by Mac and iOS.
mach_port_t ProcessMetrics::TaskForHandle(ProcessHandle process_handle) const {
  mach_port_t task = MACH_PORT_NULL;
#if BUILDFLAG(IS_MAC)
  if (port_provider_) {
    task = port_provider_->TaskForHandle(process_);
  }
#endif
  if (task == MACH_PORT_NULL && process_handle == getpid()) {
    task = mach_task_self();
  }
  return task;
}

base::expected<TimeDelta, ProcessCPUUsageError>
ProcessMetrics::GetCumulativeCPUUsage() {
  mach_port_t task = TaskForHandle(process_);
  if (task == MACH_PORT_NULL) {
    return base::unexpected(ProcessCPUUsageError::kProcessNotFound);
  }

  // Libtop explicitly loops over the threads (libtop_pinfo_update_cpu_usage()
  // in libtop.c), but this is more concise and gives the same results:
  task_thread_times_info thread_info_data;
  mach_msg_type_number_t thread_info_count = TASK_THREAD_TIMES_INFO_COUNT;
  kern_return_t kr = task_info(task, TASK_THREAD_TIMES_INFO,
                               reinterpret_cast<task_info_t>(&thread_info_data),
                               &thread_info_count);
  if (kr != KERN_SUCCESS) {
    // Most likely cause: |task| is a zombie.
    return base::unexpected(ProcessCPUUsageError::kSystemError);
  }

  const base::expected<task_basic_info_64, ProcessCPUUsageError>
      task_info_data = GetTaskInfo(task);
  if (!task_info_data.has_value()) {
    return base::unexpected(task_info_data.error());
  }

  /* Set total_time. */
  // thread info contains live time...
  struct timeval user_timeval, system_timeval, task_timeval;
  TIME_VALUE_TO_TIMEVAL(&thread_info_data.user_time, &user_timeval);
  TIME_VALUE_TO_TIMEVAL(&thread_info_data.system_time, &system_timeval);
  timeradd(&user_timeval, &system_timeval, &task_timeval);

  // ... task info contains terminated time.
  TIME_VALUE_TO_TIMEVAL(&task_info_data->user_time, &user_timeval);
  TIME_VALUE_TO_TIMEVAL(&task_info_data->system_time, &system_timeval);
  timeradd(&user_timeval, &task_timeval, &task_timeval);
  timeradd(&system_timeval, &task_timeval, &task_timeval);

  const TimeDelta measured_cpu =
      Microseconds(TimeValToMicroseconds(task_timeval));
  if (measured_cpu < last_measured_cpu_) {
    // When a thread terminates, its CPU time is immediately removed from the
    // running thread times returned by TASK_THREAD_TIMES_INFO, but there can be
    // a lag before it shows up in the terminated thread times returned by
    // GetTaskInfo(). Make sure CPU usage doesn't appear to go backwards if
    // GetCumulativeCPUUsage() is called in the interval.
    return base::ok(last_measured_cpu_);
  }
  last_measured_cpu_ = measured_cpu;
  return base::ok(measured_cpu);
}

int ProcessMetrics::GetPackageIdleWakeupsPerSecond() {
  mach_port_t task = TaskForHandle(process_);
  task_power_info power_info_data;

  GetPowerInfo(task, &power_info_data);

  // The task_power_info struct contains two wakeup counters:
  // task_interrupt_wakeups and task_platform_idle_wakeups.
  // task_interrupt_wakeups is the total number of wakeups generated by the
  // process, and is the number that Activity Monitor reports.
  // task_platform_idle_wakeups is a subset of task_interrupt_wakeups that
  // tallies the number of times the processor was taken out of its low-power
  // idle state to handle a wakeup. task_platform_idle_wakeups therefore result
  // in a greater power increase than the other interrupts which occur while the
  // CPU is already working, and reducing them has a greater overall impact on
  // power usage. See the powermetrics man page for more info.
  return CalculatePackageIdleWakeupsPerSecond(
      power_info_data.task_platform_idle_wakeups);
}

int ProcessMetrics::GetIdleWakeupsPerSecond() {
  mach_port_t task = TaskForHandle(process_);
  task_power_info power_info_data;

  GetPowerInfo(task, &power_info_data);

  return CalculateIdleWakeupsPerSecond(power_info_data.task_interrupt_wakeups);
}

// Bytes committed by the system.
size_t GetSystemCommitCharge() {
  base::apple::ScopedMachSendRight host(mach_host_self());
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  vm_statistics_data_t data;
  kern_return_t kr = host_statistics(
      host.get(), HOST_VM_INFO, reinterpret_cast<host_info_t>(&data), &count);
  if (kr != KERN_SUCCESS) {
    MACH_DLOG(WARNING, kr) << "host_statistics";
    return 0;
  }

  return (data.active_count * PAGE_SIZE) / 1024;
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  base::apple::ScopedMachSendRight host(mach_host_self());
  int result = host_info(host.get(), HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo), &count);
  if (result != KERN_SUCCESS) {
    return false;
  }

  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  meminfo->total = static_cast<int>(hostinfo.max_mem / 1024);

  vm_statistics64_data_t vm_info;
  count = HOST_VM_INFO64_COUNT;

  if (host_statistics64(host.get(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_info),
                        &count) != KERN_SUCCESS) {
    return false;
  }
  DCHECK_EQ(HOST_VM_INFO64_COUNT, count);

#if !(BUILDFLAG(IS_IOS) && defined(ARCH_CPU_X86_FAMILY))
  // PAGE_SIZE (aka vm_page_size) isn't constexpr, so this check needs to be
  // done at runtime.
  DCHECK_EQ(PAGE_SIZE % 1024, 0u) << "Invalid page size";
#else
  // On x86/x64, PAGE_SIZE used to be just a signed constant, I386_PGBYTES. When
  // Arm Macs started shipping, PAGE_SIZE was defined from day one to be
  // vm_page_size (an extern uintptr_t value), and the SDK, for x64, switched
  // PAGE_SIZE to be vm_page_size for binaries targeted for macOS 11+:
  //
  // #if !defined(__MAC_OS_X_VERSION_MIN_REQUIRED) ||
  //     (__MAC_OS_X_VERSION_MIN_REQUIRED < 101600)
  //   #define PAGE_SIZE    I386_PGBYTES
  // #else
  //   #define PAGE_SIZE    vm_page_size
  // #endif
  //
  // When building for Mac Catalyst or the iOS Simulator, this targeting
  // switcharoo breaks. Because those apps do not have a
  // __MAC_OS_X_VERSION_MIN_REQUIRED set, the SDK assumes that those apps are so
  // old that they are implicitly targeting some ancient version of macOS, and a
  // signed constant value is used for PAGE_SIZE.
  //
  // Therefore, when building for "iOS on x86", which is either Mac Catalyst or
  // the iOS Simulator, use a static assert that assumes that PAGE_SIZE is a
  // signed constant value.
  //
  // TODO(Chrome iOS team): Remove this entire #else branch when the Mac
  // Catalyst and the iOS Simulator builds only target Arm Macs.
  static_assert(PAGE_SIZE % 1024 == 0, "Invalid page size");
#endif  // !(defined(IS_IOS) && defined(ARCH_CPU_X86_FAMILY))

  if (vm_info.speculative_count <= vm_info.free_count) {
    meminfo->free = saturated_cast<int>(
        PAGE_SIZE / 1024 * (vm_info.free_count - vm_info.speculative_count));
  } else {
    // Inside the `host_statistics64` call above, `speculative_count` is
    // computed later than `free_count`, so these values are snapshots of two
    // (slightly) different points in time. As a result, it is possible for
    // `speculative_count` to have increased significantly since `free_count`
    // was computed, even to a point where `speculative_count` is greater than
    // the computed value of `free_count`. See
    // https://github.com/apple-oss-distributions/xnu/blob/aca3beaa3dfbd42498b42c5e5ce20a938e6554e5/osfmk/kern/host.c#L788
    // In this case, 0 is the best approximation for `meminfo->free`. This is
    // inexact, but even in the case where `speculative_count` is less than
    // `free_count`, the computed `meminfo->free` will only be an approximation
    // given that the two inputs come from different points in time.
    meminfo->free = 0;
  }

  meminfo->speculative =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.speculative_count);
  meminfo->file_backed =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.external_page_count);
  meminfo->purgeable =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.purgeable_count);

  return true;
}

// Both |size| and |address| are in-out parameters.
// |info| is an output parameter, only valid on Success.
MachVMRegionResult GetTopInfo(mach_port_t task,
                              mach_vm_size_t* size,
                              mach_vm_address_t* address,
                              vm_region_top_info_data_t* info) {
  mach_msg_type_number_t info_count = VM_REGION_TOP_INFO_COUNT;
  // The kernel always returns a null object for VM_REGION_TOP_INFO, but
  // balance it with a deallocate in case this ever changes. See 10.9.2
  // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
  apple::ScopedMachSendRight object_name;

  kern_return_t kr =
#if BUILDFLAG(IS_MAC)
      mach_vm_region(task, address, size, VM_REGION_TOP_INFO,
                     reinterpret_cast<vm_region_info_t>(info), &info_count,
                     apple::ScopedMachSendRight::Receiver(object_name).get());
#else
      vm_region_64(task, reinterpret_cast<vm_address_t*>(address),
                   reinterpret_cast<vm_size_t*>(size), VM_REGION_TOP_INFO,
                   reinterpret_cast<vm_region_info_t>(info), &info_count,
                   apple::ScopedMachSendRight::Receiver(object_name).get());
#endif
  return ParseOutputFromMachVMRegion(kr);
}

MachVMRegionResult GetBasicInfo(mach_port_t task,
                                mach_vm_size_t* size,
                                mach_vm_address_t* address,
                                vm_region_basic_info_64* info) {
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  // The kernel always returns a null object for VM_REGION_BASIC_INFO_64, but
  // balance it with a deallocate in case this ever changes. See 10.9.2
  // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
  apple::ScopedMachSendRight object_name;

  kern_return_t kr =
#if BUILDFLAG(IS_MAC)
      mach_vm_region(task, address, size, VM_REGION_BASIC_INFO_64,
                     reinterpret_cast<vm_region_info_t>(info), &info_count,
                     apple::ScopedMachSendRight::Receiver(object_name).get());

#else
      vm_region_64(task, reinterpret_cast<vm_address_t*>(address),
                   reinterpret_cast<vm_size_t*>(size), VM_REGION_BASIC_INFO_64,
                   reinterpret_cast<vm_region_info_t>(info), &info_count,
                   apple::ScopedMachSendRight::Receiver(object_name).get());
#endif
  return ParseOutputFromMachVMRegion(kr);
}

int ProcessMetrics::GetOpenFdCount() const {
#if BUILDFLAG(USE_BLINK)
  // In order to get a true count of the open number of FDs, PROC_PIDLISTFDS
  // is used. This is done twice: first to get the appropriate size of a
  // buffer, and then secondly to fill the buffer with the actual FD info.
  //
  // The buffer size returned in the first call is an estimate, based on the
  // number of allocated fileproc structures in the kernel. This number can be
  // greater than the actual number of open files, since the structures are
  // allocated in slabs. The value returned in proc_bsdinfo::pbi_nfiles is
  // also the number of allocated fileprocs, not the number in use.
  //
  // However, the buffer size returned in the second call is an accurate count
  // of the open number of descriptors. The contents of the buffer are unused.
  int rv = proc_pidinfo(process_, PROC_PIDLISTFDS, 0, nullptr, 0);
  if (rv < 0) {
    return -1;
  }

  base::HeapArray<char> buffer =
      base::HeapArray<char>::WithSize(static_cast<size_t>(rv));
  rv = proc_pidinfo(process_, PROC_PIDLISTFDS, 0, buffer.data(), rv);
  if (rv < 0) {
    return -1;
  }
  return static_cast<int>(static_cast<unsigned long>(rv) / PROC_PIDLISTFD_SIZE);
#else
  NOTIMPLEMENTED_LOG_ONCE();
  return -1;
#endif  // BUILDFLAG(USE_BLINK)
}

int ProcessMetrics::GetOpenFdSoftLimit() const {
  return checked_cast<int>(GetMaxFds());
}

}  // namespace base
