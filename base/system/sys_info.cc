// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <algorithm>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/features.h"
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
uint64_t GetLowMemoryDeviceThresholdMB() {
  static const uint64_t threshold = base::saturated_cast<uint64_t>(
      features::kLowMemoryDeviceThresholdMB.Get());
  return threshold;
}

std::optional<uint64_t> g_amount_of_physical_memory_mb_for_testing;
}  // namespace

// static
int SysInfo::NumberOfEfficientProcessors() {
  static int number_of_efficient_processors = NumberOfEfficientProcessorsImpl();
  return number_of_efficient_processors;
}

// static
uint64_t SysInfo::AmountOfPhysicalMemory() {
  constexpr uint64_t kMB = 1024 * 1024;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLowEndDeviceMode)) {
    // Keep using 512MB as the simulated RAM amount for when users or tests have
    // manually enabled low-end device mode. Note this value is different from
    // the threshold used for low end devices.
    constexpr uint64_t kSimulatedMemoryForEnableLowEndDeviceMode = 512 * kMB;
    return std::min(kSimulatedMemoryForEnableLowEndDeviceMode,
                    AmountOfPhysicalMemoryImpl());
  }

  if (g_amount_of_physical_memory_mb_for_testing) {
    return g_amount_of_physical_memory_mb_for_testing.value() * kMB;
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
    uint64_t memory_limit = GetLowMemoryDeviceThresholdMB() * 1024 * 1024;
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

namespace {

enum class BucketizedSize {
  k2GbOrLess,
  k3Gb,
  k4Gb,
  k6Gb,
  k8GbOrHigher,
};

BucketizedSize GetSystemRamBucketizedSize() {
  int physical_memory = base::SysInfo::AmountOfPhysicalMemoryMB();

  // Because of Android carveouts, AmountOfPhysicalMemory() returns smaller
  // than the actual memory size, So we will use a small lowerbound than "X"GB
  // to discriminate real "X"GB devices from lower memory ones.
  // Addendum: This logic should also work for ChromeOS.

  constexpr int kUpperBound2GB = 2 * 1024;  // inclusive
  if (physical_memory <= kUpperBound2GB) {
    return BucketizedSize::k2GbOrLess;
  }

  constexpr int kLowerBound3GB = kUpperBound2GB;  // exclusive
  constexpr int kUpperBound3GB = 3.2 * 1024;      // inclusive
  if (kLowerBound3GB < physical_memory && physical_memory <= kUpperBound3GB) {
    return BucketizedSize::k3Gb;
  }

  constexpr int kLowerBound4GB = kUpperBound3GB;  // exclusive
  constexpr int kUpperBound4GB = 4 * 1024;        // inclusive
  if (kLowerBound4GB < physical_memory && physical_memory <= kUpperBound4GB) {
    return BucketizedSize::k4Gb;
  }

  constexpr int kLowerBound6GB = kUpperBound4GB;  // exclusive
  constexpr int kUpperBound6GB = 6.5 * 1024 - 1;  // inclusive
  if (kLowerBound6GB < physical_memory && physical_memory <= kUpperBound6GB) {
    return BucketizedSize::k6Gb;
  }

  return BucketizedSize::k8GbOrHigher;
}

BucketizedSize GetCachedSystemRamBucketizedSize() {
  static BucketizedSize s_size = GetSystemRamBucketizedSize();
  return s_size;
}

bool IsPartialLowEndModeOnMidRangeDevicesEnabled() {
  // TODO(crbug.com/40264947): make the feature not enable on 32-bit devices
  // before launching or going to high Stable %.
  return SysInfo::Is4GbOr6GbDevice() &&
         base::FeatureList::IsEnabled(
             features::kPartialLowEndModeOnMidRangeDevices);
}

bool IsPartialLowEndModeOn3GbDevicesEnabled() {
  return SysInfo::Is3GbDevice() &&
         base::FeatureList::IsEnabled(features::kPartialLowEndModeOn3GbDevices);
}

}  // namespace

bool SysInfo::Is3GbDevice() {
  return GetCachedSystemRamBucketizedSize() == BucketizedSize::k3Gb;
}

bool SysInfo::Is4GbDevice() {
  return GetCachedSystemRamBucketizedSize() == BucketizedSize::k4Gb;
}

bool SysInfo::Is4GbOr6GbDevice() {
  return GetCachedSystemRamBucketizedSize() == BucketizedSize::k4Gb ||
         GetCachedSystemRamBucketizedSize() == BucketizedSize::k6Gb;
}

bool SysInfo::Is6GbDevice() {
  return GetCachedSystemRamBucketizedSize() == BucketizedSize::k6Gb;
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

// TODO(crbug.com/40264947): This method is for chromium native code.
// We need to update the java-side code, i.e.
// base/android/java/src/org/chromium/base/SysUtils.java,
// and to make the selected components in java to see this feature.
bool SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return base::SysInfo::IsLowEndDevice() ||
         IsPartialLowEndModeOnMidRangeDevicesEnabled() ||
         IsPartialLowEndModeOn3GbDevicesEnabled();
#else
  return base::SysInfo::IsLowEndDevice();
#endif
}

bool SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled(
    const FeatureParam<bool>& param_for_exclusion) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return base::SysInfo::IsLowEndDevice() ||
         ((IsPartialLowEndModeOnMidRangeDevicesEnabled() ||
           IsPartialLowEndModeOn3GbDevicesEnabled()) &&
          !param_for_exclusion.Get());
#else
  return base::SysInfo::IsLowEndDevice();
#endif
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
         static_cast<uint64_t>(ram_size_mb) <= GetLowMemoryDeviceThresholdMB();
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

#if !BUILDFLAG(IS_ANDROID)
std::string SysInfo::SocManufacturer() {
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
#elif defined(ARCH_CPU_RISCV64)
  return "RISCV_64";
#else
  return std::string();
#endif
}

// static
std::optional<uint64_t> SysInfo::SetAmountOfPhysicalMemoryMbForTesting(
    const uint64_t amount_of_memory_mb) {
  std::optional<uint64_t> current = g_amount_of_physical_memory_mb_for_testing;
  g_amount_of_physical_memory_mb_for_testing.emplace(amount_of_memory_mb);
  return current;
}

// static
void SysInfo::ClearAmountOfPhysicalMemoryMbForTesting() {
  g_amount_of_physical_memory_mb_for_testing.reset();
}

}  // namespace base
