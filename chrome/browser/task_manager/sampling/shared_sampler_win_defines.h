// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_SHARED_SAMPLER_WIN_DEFINES_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_SHARED_SAMPLER_WIN_DEFINES_H_

#include <windows.h>
#include <winternl.h>

#include "build/build_config.h"

namespace task_manager {

// From <wdm.h>
typedef LONG KPRIORITY;
typedef LONG KWAIT_REASON;  // Full definition is in wdm.h

// From ntddk.h
typedef struct _VM_COUNTERS {
  SIZE_T PeakVirtualSize;
  SIZE_T VirtualSize;
  ULONG PageFaultCount;
  // Padding here in 64-bit
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
} VM_COUNTERS;

// Two possibilities available from here:
// http://stackoverflow.com/questions/28858849/where-is-system-information-class-defined

typedef enum _SYSTEM_INFORMATION_CLASS {
  SystemProcessInformation = 5,  // This is the number that we need.
} SYSTEM_INFORMATION_CLASS;

// https://msdn.microsoft.com/en-us/library/gg750647.aspx?f=255&MSPPError=-2147217396
typedef struct {
  HANDLE UniqueProcess;  // Actually process ID
  HANDLE UniqueThread;   // Actually thread ID
} CLIENT_ID;

// From http://alax.info/blog/1182, with corrections and modifications
// Originally from
// http://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FSystem%20Information%2FStructures%2FSYSTEM_THREAD.html
struct SYSTEM_THREAD_INFORMATION {
  ULONGLONG KernelTime;
  ULONGLONG UserTime;
  ULONGLONG CreateTime;
  ULONG WaitTime;
  // Padding here in 64-bit
  PVOID StartAddress;
  CLIENT_ID ClientId;
  KPRIORITY Priority;
  LONG BasePriority;
  ULONG ContextSwitchCount;
  ULONG State;
  KWAIT_REASON WaitReason;
};
#if defined(ARCH_CPU_64_BITS)
static_assert(sizeof(SYSTEM_THREAD_INFORMATION) == 80,
              "Structure size mismatch");
#else
static_assert(sizeof(SYSTEM_THREAD_INFORMATION) == 64,
              "Structure size mismatch");
#endif

// From http://alax.info/blog/1182, with corrections and modifications
// Originally from
// http://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FSystem%20Information%2FStructures%2FSYSTEM_THREAD.html
struct SYSTEM_PROCESS_INFORMATION {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  // http://processhacker.sourceforge.net/doc/struct___s_y_s_t_e_m___p_r_o_c_e_s_s___i_n_f_o_r_m_a_t_i_o_n.html
  ULONGLONG WorkingSetPrivateSize;
  ULONG HardFaultCount;
  ULONG Reserved1;
  ULONGLONG CycleTime;
  ULONGLONG CreateTime;
  ULONGLONG UserTime;
  ULONGLONG KernelTime;
  UNICODE_STRING ImageName;
  KPRIORITY BasePriority;
  HANDLE ProcessId;
  HANDLE ParentProcessId;
  ULONG HandleCount;
  ULONG Reserved2[2];
  // Padding here in 64-bit
  VM_COUNTERS VirtualMemoryCounters;
  size_t Reserved3;
  IO_COUNTERS IoCounters;
  SYSTEM_THREAD_INFORMATION Threads[1];
};
#if defined(ARCH_CPU_64_BITS)
static_assert(sizeof(SYSTEM_PROCESS_INFORMATION) == 336,
              "Structure size mismatch");
#else
static_assert(sizeof(SYSTEM_PROCESS_INFORMATION) == 248,
              "Structure size mismatch");
#endif

typedef NTSTATUS(WINAPI* NTQUERYSYSTEMINFORMATION)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

}  // namespace task_manager

#endif  //  CHROME_BROWSER_TASK_MANAGER_SAMPLING_SHARED_SAMPLER_WIN_DEFINES_H_
