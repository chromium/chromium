// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <algorithm>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/system/sys_info_internal.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {
namespace {
#if BUILDFLAG(IS_IOS)
// For M99, 45% of devices have 2GB of RAM, and 55% have more.
constexpr uint64_t kLowMemoryDeviceThresholdMB = 1024;
#else
// Updated Desktop default threshold to match the Android 2021 definition.
constexpr uint64_t kLowMemoryDeviceThresholdMB = 2048;
#endif
}  // namespace

// static
int SysInfo::NumberOfEfficientProcessors() {
  static int number_of_efficient_processors = NumberOfEfficientProcessorsImpl();
  return number_of_efficient_processors;
}

// static
uint64_t SysInfo::AmountOfPhysicalMemory() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLowEndDeviceMode)) {
    // Keep using 512MB as the simulated RAM amount for when users or tests have
    // manually enabled low-end device mode. Note this value is different from
    // the threshold used for low end devices.
    constexpr uint64_t kSimulatedMemoryForEnableLowEndDeviceMode =
        512 * 1024 * 1024;
    return std::min(kSimulatedMemoryForEnableLowEndDeviceMode,
                    AmountOfPhysicalMemoryImpl());
  }

  return AmountOfPhysicalMemoryImpl();
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemory() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLowEndDeviceMode)) {
    // Estimate the available memory by subtracting our memory used estimate
    // from the fake |kLowMemoryDeviceThresholdMB| limit.
    uint64_t memory_used =
        AmountOfPhysicalMemoryImpl() - AmountOfAvailablePhysicalMemoryImpl();
    uint64_t memory_limit = kLowMemoryDeviceThresholdMB * 1024 * 1024;
    // std::min ensures no underflow, as |memory_used| can be > |memory_limit|.
    return memory_limit - std::min(memory_used, memory_limit);
  }

  return AmountOfAvailablePhysicalMemoryImpl();
}

bool SysInfo::IsLowEndDevice() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLowEndDeviceMode)) {
    return true;
  }

  return IsLowEndDeviceImpl();
}

#if !BUILDFLAG(IS_ANDROID)
// The Android equivalent of this lives in `detectLowEndDevice()` at:
// base/android/java/src/org/chromium/base/SysUtils.java
bool DetectLowEndDevice() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableLowEndDeviceMode))
    return true;
  if (command_line->HasSwitch(switches::kDisableLowEndDeviceMode))
    return false;

  int ram_size_mb = SysInfo::AmountOfPhysicalMemoryMB();
  return ram_size_mb > 0 &&
         static_cast<uint64_t>(ram_size_mb) <= kLowMemoryDeviceThresholdMB;
}

// static
bool SysInfo::IsLowEndDeviceImpl() {
  static internal::LazySysInfoValue<bool, DetectLowEndDevice> instance;
  return instance.value();
}
#endif

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN) && \
    !BUILDFLAG(IS_CHROMEOS)
std::string SysInfo::HardwareModelName() {
  return std::string();
}
#endif

void SysInfo::GetHardwareInfo(base::OnceCallback<void(HardwareInfo)> callback) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  constexpr base::TaskTraits kTraits = {base::MayBlock()};
#else
  constexpr base::TaskTraits kTraits = {};
#endif

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTraits, base::BindOnce(&GetHardwareInfoSync),
      std::move(callback));
}

// static
base::TimeDelta SysInfo::Uptime() {
  // This code relies on an implementation detail of TimeTicks::Now() - that
  // its return value happens to coincide with the system uptime value in
  // microseconds, on Win/Mac/iOS/Linux/ChromeOS and Android.
  int64_t uptime_in_microseconds = TimeTicks::Now().ToInternalValue();
  return base::Microseconds(uptime_in_microseconds);
}

// static
std::string SysInfo::ProcessCPUArchitecture() {
#if defined(ARCH_CPU_X86)
  return "x86";
#elif defined(ARCH_CPU_X86_64)
  return "x86_64";
#elif defined(ARCH_CPU_ARMEL)
  return "ARM";
#elif defined(ARCH_CPU_ARM64)
  return "ARM_64";
#else
  return std::string();
#endif
}

}  // namespace base
