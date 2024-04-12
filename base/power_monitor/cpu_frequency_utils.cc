// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/cpu_frequency_utils.h"

#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <powerbase.h>
#include <winternl.h>
#endif

namespace base {
namespace {

#if BUILDFLAG(IS_WIN)
// From
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa373184(v=vs.85).aspx.
// Note that this structure definition was accidentally omitted from WinNT.h.
typedef struct _PROCESSOR_POWER_INFORMATION {
  ULONG Number;
  ULONG MaxMhz;
  ULONG CurrentMhz;
  ULONG MhzLimit;
  ULONG MaxIdleState;
  ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif

}  // namespace

double EstimateCpuFrequency() {
#if defined(ARCH_CPU_X86_FAMILY)
  // The heuristic to estimate CPU frequency is based on UIforETW code.
  // see: https://github.com/google/UIforETW/blob/main/UIforETW/CPUFrequency.cpp
  //      https://github.com/google/UIforETW/blob/main/UIforETW/SpinALot64.asm
  base::ElapsedTimer timer;
  const int kAmountOfIterations = 50000;
  const int kAmountOfInstructions = 10;
  for (int i = 0; i < kAmountOfIterations; ++i) {
    __asm__ __volatile__(
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        "addl  %%eax, %%eax\n"
        :
        :
        : "eax");
  }

  const base::TimeDelta elapsed = timer.Elapsed();
  const double estimated_frequency =
      (kAmountOfIterations * kAmountOfInstructions) / elapsed.InSecondsF();
  return estimated_frequency;
#else
  return 0.0;
#endif
}

unsigned long GetCpuMaxMhz() {
#if BUILDFLAG(IS_WIN)
  size_t num_cpu = static_cast<size_t>(base::SysInfo::NumberOfProcessors());
  std::vector<PROCESSOR_POWER_INFORMATION> info(num_cpu);
  if (!NT_SUCCESS(CallNtPowerInformation(
          ProcessorInformation, nullptr, 0, &info[0],
          static_cast<ULONG>(sizeof(PROCESSOR_POWER_INFORMATION) * num_cpu)))) {
    return 0;
  }

  unsigned long max_mhz = 0;
  for (const auto& i : info) {
    max_mhz = std::max(max_mhz, i.MaxMhz);
  }

  return max_mhz;
#else
  return 0;
#endif
}

unsigned long GetCpuMhzLimit() {
#if BUILDFLAG(IS_WIN)
  size_t num_cpu = static_cast<size_t>(base::SysInfo::NumberOfProcessors());
  std::vector<PROCESSOR_POWER_INFORMATION> info(num_cpu);
  if (!NT_SUCCESS(CallNtPowerInformation(
          ProcessorInformation, nullptr, 0, &info[0],
          static_cast<ULONG>(sizeof(PROCESSOR_POWER_INFORMATION) * num_cpu)))) {
    return 0;
  }

  unsigned long mhz_limit = 0;
  for (const auto& i : info) {
    mhz_limit = std::max(mhz_limit, i.MhzLimit);
  }

  return mhz_limit;
#else
  return 0;
#endif
}

}  // namespace base
