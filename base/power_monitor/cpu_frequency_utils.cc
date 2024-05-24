// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/cpu_frequency_utils.h"

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <powerbase.h>
#include <processthreadsapi.h>
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
  std::optional<CpuThroughputEstimationResult> result = EstimateCpuThroughput();
  return result ? result->estimated_frequency : 0.0;
}

std::optional<CpuThroughputEstimationResult> EstimateCpuThroughput() {
#if defined(ARCH_CPU_X86_FAMILY)
  TRACE_EVENT0("power", "EstimateCpuThroughput");

#if BUILDFLAG(IS_WIN)
  DWORD start_processor_number = GetCurrentProcessorNumber();
#endif

  // The heuristic to estimate CPU frequency is based on UIforETW code.
  // see: https://github.com/google/UIforETW/blob/main/UIforETW/CPUFrequency.cpp
  //      https://github.com/google/UIforETW/blob/main/UIforETW/SpinALot64.asm
  base::ElapsedTimer timer;
  base::ElapsedThreadTimer thread_timer;
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

  const base::TimeDelta elapsed_thread_time = thread_timer.Elapsed();
  const base::TimeDelta elapsed = timer.Elapsed();
  const double estimated_frequency =
      (kAmountOfIterations * kAmountOfInstructions) / elapsed.InSecondsF();

  CpuThroughputEstimationResult result{
      .estimated_frequency = estimated_frequency,
      .migrated = false,
      .wall_time = elapsed,
      .thread_time = elapsed_thread_time,
  };

#if BUILDFLAG(IS_WIN)
  result.migrated = start_processor_number != GetCurrentProcessorNumber();
#endif

  return result;
#else
  return std::nullopt;
#endif
}

BASE_EXPORT CpuFrequencyInfo GetCpuFrequencyInfo() {
  CpuFrequencyInfo cpu_info{
      .max_mhz = 0,
      .mhz_limit = 0,
      .type = CpuFrequencyInfo::CoreType::kPerformance,
  };

#if BUILDFLAG(IS_WIN)
  unsigned long fastest = std::numeric_limits<unsigned long>::min();
  unsigned long slowest = std::numeric_limits<unsigned long>::max();

  DWORD current_processor_number = GetCurrentProcessorNumber();
  size_t num_cpu = static_cast<size_t>(base::SysInfo::NumberOfProcessors());
  std::vector<PROCESSOR_POWER_INFORMATION> info(num_cpu);
  if (!NT_SUCCESS(CallNtPowerInformation(
          ProcessorInformation, nullptr, 0, &info[0],
          static_cast<ULONG>(sizeof(PROCESSOR_POWER_INFORMATION) * num_cpu)))) {
    return cpu_info;
  }

  for (const auto& i : info) {
    if (current_processor_number == i.Number) {
      cpu_info.max_mhz = i.MaxMhz;
      cpu_info.mhz_limit = i.MhzLimit;
    }
    fastest = std::max(fastest, i.MaxMhz);
    slowest = std::min(slowest, i.MaxMhz);
  }

  // If the CPU frequency is the fastest of all the cores, or the CPU is
  // homogeneous, report the core as being a performance core.
  if (cpu_info.max_mhz == fastest) {
    cpu_info.type = CpuFrequencyInfo::CoreType::kPerformance;
  } else if (cpu_info.max_mhz == slowest) {
    // If the system is heterogenous, and the current CPU is the slowest, report
    // it as an efficiency core.
    cpu_info.type = CpuFrequencyInfo::CoreType::kEfficiency;
  } else {
    // Otherwise, the CPU is neither the fastest or the slowest, so report it as
    // "balanced".
    cpu_info.type = CpuFrequencyInfo::CoreType::kBalanced;
  }
#endif

  return cpu_info;
}

#if BUILDFLAG(IS_WIN)
void GenerateCpuInfoForTracingMetadata(base::Value::Dict* metadata) {
  size_t num_cpu = static_cast<size_t>(base::SysInfo::NumberOfProcessors());
  std::vector<PROCESSOR_POWER_INFORMATION> info(num_cpu);
  if (!NT_SUCCESS(CallNtPowerInformation(
          ProcessorInformation, nullptr, 0, &info[0],
          static_cast<ULONG>(sizeof(PROCESSOR_POWER_INFORMATION) * num_cpu)))) {
    return;
  }

  // Output information for each cores. The cores frequencies may differ due to
  // little/big cores.
  for (const auto& i : info) {
    const ULONG cpu_number = i.Number;

    // The maximum CPU frequency for a given core.
    metadata->Set(base::StringPrintf("cpu-max-frequency-core%lu", cpu_number),
                  static_cast<int>(i.MaxMhz));

    // The maximum CPU frequency that the power settings will allow. This
    // setting can be changed by the users or by changing the power plan.
    if (i.MhzLimit != i.MaxMhz) {
      metadata->Set(
          base::StringPrintf("cpu-limit-frequency-core%lu", cpu_number),
          static_cast<int>(i.MhzLimit));
    }

    // The MaxIdleState field contains the maximum supported C-state. The value
    // is zero when the C-State is not supported.
    if (i.MaxIdleState != 0) {
      metadata->Set(
          base::StringPrintf("cpu-max-idle-state-core%lu", cpu_number),
          static_cast<int>(i.MaxIdleState));
    }
  }
}
#endif

}  // namespace base
