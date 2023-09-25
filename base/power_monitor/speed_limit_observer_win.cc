// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/speed_limit_observer_win.h"

#include <windows.h>

#include <powerbase.h>
#include <winternl.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/system/sys_info.h"

namespace {

// From ntdef.f
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// We poll for new speed-limit values once every second.
constexpr base::TimeDelta kSampleInterval = base::Seconds(1);

// Size of moving-average filter which is used to smooth out variations in
// speed-limit estimates.
constexpr size_t kMovingAverageWindowSize = 10;

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

// From
// https://docs.microsoft.com/en-us/windows/win32/power/system-power-information-str.
// Note that this structure definition was accidentally omitted from WinNT.h.
typedef struct _SYSTEM_POWER_INFORMATION {
  ULONG MaxIdlenessAllowed;
  ULONG Idleness;
  ULONG TimeRemaining;
  UCHAR CoolingMode;
} SYSTEM_POWER_INFORMATION, *PSYSTEM_POWER_INFORMATION;

// Returns information about the idleness of the system.
bool GetCPUIdleness(int* idleness_percent) {
  auto info = std::make_unique<SYSTEM_POWER_INFORMATION>();
  if (!NT_SUCCESS(CallNtPowerInformation(SystemPowerInformation, nullptr, 0,
                                         info.get(),
                                         sizeof(SYSTEM_POWER_INFORMATION)))) {
    *idleness_percent = 0;
    return false;
  }
  // The current idle level, expressed as a percentage.
  *idleness_percent = static_cast<int>(info->Idleness);
  return true;
}

}  // namespace

namespace base {

SpeedLimitObserverWin::SpeedLimitObserverWin(
    SpeedLimitUpdateCallback speed_limit_update_callback)
    : callback_(std::move(speed_limit_update_callback)),
      num_cpus_(static_cast<size_t>(SysInfo::NumberOfProcessors())),
      moving_average_(kMovingAverageWindowSize) {
  DVLOG(1) << __func__ << "(num_CPUs=" << num_cpus() << ")";
  timer_.Start(FROM_HERE, kSampleInterval, this,
               &SpeedLimitObserverWin::OnTimerTick);
}

SpeedLimitObserverWin::~SpeedLimitObserverWin() {
  timer_.Stop();
}

int SpeedLimitObserverWin::GetCurrentSpeedLimit() {
  const int kSpeedLimitMax = PowerThermalObserver::kSpeedLimitMax;

  int idleness_percent = 0;
  if (!GetCPUIdleness(&idleness_percent)) {
    DLOG(WARNING) << "GetCPUIdleness failed";
    return kSpeedLimitMax;
  }

  // Get the latest estimated throttling level (value between 0.0 and 1.0).
  float throttling_level = EstimateThrottlingLevel();

  // Ignore the value if the global idleness is above 90% or throttling value
  // is very small. This approach avoids false alarms and removes noise from the
  // measurements.
  if (idleness_percent > 90 || throttling_level < 0.1f) {
    moving_average_.Reset();
    return kSpeedLimitMax;
  }

  // The speed limit metric is a value between 0 and 100 [%] where 100 means
  // "full speed". The corresponding UMA metric is CPU_Speed_Limit.
  float speed_limit_factor = 1.0f - throttling_level;
  int speed_limit =
      static_cast<int>(std::ceil(kSpeedLimitMax * speed_limit_factor));

  // The previous speed-limit value was below 100 but the new value is now back
  // at max again. To make this state more "stable or sticky" we reset the MA
  // filter and return kSpeedLimitMax. As a result, single drops in speedlimit
  // values will not result in a value less than 100 since the MA filter must
  // be full before we start to produce any output.
  if (speed_limit_ < kSpeedLimitMax && speed_limit == kSpeedLimitMax) {
    moving_average_.Reset();
    return kSpeedLimitMax;
  }

  // Add the latest speeed-limit value [0,100] to the MA filter and return its
  // output after ensuring that the filter is full. We do this to avoid initial
  // false alarms at startup and after calling Reset() on the filter.
  moving_average_.AddSample(speed_limit);
  if (moving_average_.Count() < kMovingAverageWindowSize) {
    return kSpeedLimitMax;
  }
  return moving_average_.Mean();
}

void SpeedLimitObserverWin::OnTimerTick() {
  // Get the latest (filtered) speed-limit estimate and trigger a new callback
  // if the new value is different from the last.
  const int speed_limit = GetCurrentSpeedLimit();
  if (speed_limit != speed_limit_) {
    speed_limit_ = speed_limit;
    callback_.Run(speed_limit_);
  }
}

float SpeedLimitObserverWin::EstimateThrottlingLevel() {
  float throttling_level = 0.f;

  // Populate the PROCESSOR_POWER_INFORMATION structures for all logical CPUs
  // using the CallNtPowerInformation API.
  std::vector<PROCESSOR_POWER_INFORMATION> info(num_cpus());
  if (!NT_SUCCESS(CallNtPowerInformation(
          ProcessorInformation, nullptr, 0, &info[0],
          static_cast<ULONG>(sizeof(PROCESSOR_POWER_INFORMATION) *
                             num_cpus())))) {
    return throttling_level;
  }

  // Estimate the level of throttling by measuring how many CPUs that are not
  // in idle state and how "far away" they are from the most idle state. Local
  // tests have shown that `MaxIdleState` is typically 2 or 3 and
  // `CurrentIdleState` switches to 2 or 1 when some sort of trottling starts
  // to take place. `CurrentIdleState` equal to 0 can happen on devices where
  // `MaxIdleState` equals 1 but it seems hard to provoke when `MaxIdleState`
  // is larger than 1.
  // The Intel Extreme Tuning Utility application has been used to monitor when
  // any type of throttling (thermal, power-limit, PMAX etc) starts.
  int num_non_idle_cpus = 0;
  float load_fraction_total = 0.0;
  for (size_t i = 0; i < num_cpus(); ++i) {
    // Amount of "non-idleness" is the distance from the max idle state.
    const auto idle_diff = info[i].MaxIdleState - info[i].CurrentIdleState;
    // Derive a value between 0.0 and 1.0 where 1.0 corresponds to max load on
    // CPU#i.
    // Example: MaxIdleState=2, CurrentIdleState=1 => (2 - 1) / 2 = 0.5.
    // Example: MaxIdleState=2, CurrentIdleState=2 => (2 - 2) / 2 = 1.0.
    // Example: MaxIdleState=3, CurrentIdleState=1 => (3 - 1) / 3 = 0.6666.
    // Example: MaxIdleState=3, CurrentIdleState=2 => (3 - 2) / 3 = 0.3333.
    const float load_fraction =
        static_cast<float>(idle_diff) / info[i].MaxIdleState;
    // Accumulate the total load for all CPUs.
    load_fraction_total += load_fraction;
    // Used for a sanity check only.
    num_non_idle_cpus += (info[i].CurrentIdleState < info[i].MaxIdleState);
  }
  DCHECK_LE(load_fraction_total, static_cast<float>(num_non_idle_cpus))
      << " load_fraction_total: " << load_fraction_total
      << " num_non_idle_cpus:" << num_non_idle_cpus;
  throttling_level = (load_fraction_total / num_cpus());
  return throttling_level;
}

}  // namespace base
