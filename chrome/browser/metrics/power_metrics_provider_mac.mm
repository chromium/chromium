// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power_metrics_provider_mac.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <libkern/OSByteOrder.h>

#include "base/bind.h"
#include "base/mac/scoped_ioobject.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_finder.h"

namespace {
constexpr base::TimeDelta kStartupPowerMetricsCollectionDuration =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kStartupPowerMetricsCollectionInterval =
    base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kPostStartupPowerMetricsCollectionInterval =
    base::TimeDelta::FromSeconds(60);

// This API is undocumented. It can read hardware sensors including
// temperature, voltage, and power. A useful tool for discovering new keys is
// <https://github.com/theopolis/smc-fuzzer>. The following definitions are
// from
// <https://opensource.apple.com/source/PowerManagement/PowerManagement-271.1.1/pmconfigd/PrivateLib.c.auto.html>.
struct SMCParamStruct {
  enum {
    kSMCUserClientOpen = 0,
    kSMCUserClientClose = 1,
    kSMCHandleYPCEvent = 2,
    kSMCReadKey = 5,
    kSMCGetKeyInfo = 9,
  };

  enum class SMCKey : uint32_t {
    TotalPower = 'PSTR',  // Power: System Total Rail (watts)
    CPUPower = 'PCPC',    // Power: CPU Package CPU (watts)
    iGPUPower = 'PCPG',   // Power: CPU Package GPU (watts)
    GPU0Power = 'PG0R',   // Power: GPU 0 Rail (watts)
    GPU1Power = 'PG1R',   // Power: GPU 1 Rail (watts)
  };

  // SMC keys are typed, and there are a number of numeric types. Support for
  // decoding the ones in this enum is implemented below, but there are more
  // types (and more may appear in future hardware). Implement as needed.
  enum class DataType : uint32_t {
    flt = 'flt ',   // Floating point
    sp78 = 'sp78',  // Fixed point: SIIIIIIIFFFFFFFF
    sp87 = 'sp87',  // Fixed point: SIIIIIIIIFFFFFFF
    spa5 = 'spa5',  // Fixed point: SIIIIIIIIIIFFFFF
  };

  struct SMCVersion {
    unsigned char major;
    unsigned char minor;
    unsigned char build;
    unsigned char reserved;
    unsigned short release;
  };

  struct SMCPLimitData {
    uint16_t version;
    uint16_t length;
    uint32_t cpuPLimit;
    uint32_t gpuPLimit;
    uint32_t memPLimit;
  };

  struct SMCKeyInfoData {
    IOByteCount dataSize;
    DataType dataType;
    uint8_t dataAttributes;
  };

  SMCKey key;
  SMCVersion vers;
  SMCPLimitData pLimitData;
  SMCKeyInfoData keyInfo;
  uint8_t result;
  uint8_t status;
  uint8_t data8;
  uint32_t data32;
  uint8_t bytes[32];
};

float FromSMCFixedPoint(uint8_t* bytes, size_t fraction_bits) {
  return static_cast<int16_t>(OSReadBigInt16(bytes, 0)) /
         static_cast<float>(1 << fraction_bits);
}

class SMCKey {
 public:
  SMCKey(base::mac::ScopedIOObject<io_object_t> connect,
         SMCParamStruct::SMCKey key)
      : connect_(std::move(connect)), key_(key) {
    SMCParamStruct out{};
    if (CallSMCFunction(SMCParamStruct::kSMCGetKeyInfo, &out))
      keyInfo_ = out.keyInfo;
  }

  bool Exists() { return keyInfo_.dataSize > 0; }

  float Read() {
    if (!Exists())
      return 0;

    SMCParamStruct out{};
    if (!CallSMCFunction(SMCParamStruct::kSMCReadKey, &out))
      return 0;
    switch (keyInfo_.dataType) {
      case SMCParamStruct::DataType::flt:
        return *reinterpret_cast<float*>(out.bytes);
      case SMCParamStruct::DataType::sp78:
        return FromSMCFixedPoint(out.bytes, 8);
      case SMCParamStruct::DataType::sp87:
        return FromSMCFixedPoint(out.bytes, 7);
      case SMCParamStruct::DataType::spa5:
        return FromSMCFixedPoint(out.bytes, 5);
      default:
        break;
    }
    return 0;
  }

 private:
  bool CallSMCFunction(uint8_t which, SMCParamStruct* out) {
    if (!connect_)
      return false;
    if (IOConnectCallMethod(connect_, SMCParamStruct::kSMCUserClientOpen,
                            nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr,
                            nullptr)) {
      connect_.reset();
      return false;
    }

    SMCParamStruct in{};
    in.key = key_;
    in.keyInfo.dataSize = keyInfo_.dataSize;
    in.data8 = which;

    size_t out_size = sizeof(*out);
    bool success = IOConnectCallStructMethod(
                       connect_, SMCParamStruct::kSMCHandleYPCEvent, &in,
                       sizeof(in), out, &out_size) == kIOReturnSuccess;

    if (IOConnectCallMethod(connect_, SMCParamStruct::kSMCUserClientClose,
                            nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr,
                            nullptr))
      connect_.reset();

    // Even if the close failed, report whether the actual call succeded.
    return success;
  }

  base::mac::ScopedIOObject<io_object_t> connect_;
  SMCParamStruct::SMCKey key_;
  SMCParamStruct::SMCKeyInfoData keyInfo_{};
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ThermalStateUMA {
  kNominal = 0,
  kFair = 1,
  kSerious = 2,
  kCritical = 3,
  kMaxValue = kCritical,
};

ThermalStateUMA ThermalStateToUmaEnumValue(NSProcessInfoThermalState state)
    API_AVAILABLE(macos(10.10.3)) {
  switch (state) {
    case NSProcessInfoThermalStateNominal:
      return ThermalStateUMA::kNominal;
    case NSProcessInfoThermalStateFair:
      return ThermalStateUMA::kFair;
    case NSProcessInfoThermalStateSerious:
      return ThermalStateUMA::kSerious;
    case NSProcessInfoThermalStateCritical:
      return ThermalStateUMA::kCritical;
  }
}

}  // namespace

class PowerMetricsProvider::Impl : public base::RefCountedThreadSafe<Impl> {
 public:
  static scoped_refptr<Impl> Create(
      base::mac::ScopedIOObject<io_object_t> connect) {
    scoped_refptr<Impl> impl = new Impl(std::move(connect));
    impl->ScheduleCollection();
    return impl;
  }

 private:
  friend class base::RefCountedThreadSafe<Impl>;
  Impl(base::mac::ScopedIOObject<io_object_t> connect)
      : task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        system_total_power_key_(connect, SMCParamStruct::SMCKey::TotalPower),
        cpu_package_cpu_power_key_(connect, SMCParamStruct::SMCKey::CPUPower),
        cpu_package_gpu_power_key_(connect, SMCParamStruct::SMCKey::iGPUPower),
        gpu_0_power_key_(connect, SMCParamStruct::SMCKey::GPU0Power),
        gpu_1_power_key_(connect, SMCParamStruct::SMCKey::GPU0Power) {}

  ~Impl() = default;

  bool IsInStartup() {
    if (could_be_in_startup_) {
      const base::TimeDelta process_uptime =
          base::Time::Now() - base::Process::Current().CreationTime();
      if (process_uptime >= kStartupPowerMetricsCollectionDuration)
        could_be_in_startup_ = false;
    }
    return could_be_in_startup_;
  }

  void ScheduleCollection() {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&Impl::Collect, this),
        IsInStartup() ? kStartupPowerMetricsCollectionInterval
                      : kPostStartupPowerMetricsCollectionInterval);
  }

  void Collect() {
    ScheduleCollection();

    if (IsInStartup()) {
      RecordSMC("DuringStartup");
    } else {
      RecordSMC("All");
      RecordIsOnBattery();
      if (@available(macOS 10.10.3, *)) {
        RecordThermal();
      }
    }
  }

  void RecordSMC(const std::string& name) {
    const struct {
      const char* uma_prefix;
      SMCKey& smc_key;
    } sensors[] = {
        {"Power.Mac.Total.", system_total_power_key_},
        {"Power.Mac.CPU.", cpu_package_cpu_power_key_},
        {"Power.Mac.GPUi.", cpu_package_gpu_power_key_},
        {"Power.Mac.GPU0.", gpu_0_power_key_},
        {"Power.Mac.GPU1.", gpu_1_power_key_},
    };
    for (const auto& sensor : sensors) {
      if (sensor.smc_key.Exists()) {
        if (auto power_mw = sensor.smc_key.Read() * 1000)
          base::UmaHistogramCounts100000(sensor.uma_prefix + name, power_mw);
      }
    }
  }

  void RecordIsOnBattery() {
    bool is_on_battery = false;
    if (base::PowerMonitor::IsInitialized())
      is_on_battery = base::PowerMonitor::IsOnBatteryPower();
    UMA_HISTOGRAM_BOOLEAN("Power.Mac.IsOnBattery", is_on_battery);
  }

  void RecordThermal() API_AVAILABLE(macos(10.10.3)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Power.Mac.ThermalState",
        ThermalStateToUmaEnumValue([[NSProcessInfo processInfo] thermalState]));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool could_be_in_startup_ = true;

  SMCKey system_total_power_key_;
  SMCKey cpu_package_cpu_power_key_;
  SMCKey cpu_package_gpu_power_key_;
  SMCKey gpu_0_power_key_;
  SMCKey gpu_1_power_key_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

PowerMetricsProvider::PowerMetricsProvider() = default;
PowerMetricsProvider::~PowerMetricsProvider() = default;

void PowerMetricsProvider::OnRecordingEnabled() {
  const base::mac::ScopedIOObject<io_service_t> smc_service(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("AppleSMC")));
  io_object_t connect;
  bool service_opened = IOServiceOpen(smc_service, mach_task_self(), 1,
                                      &connect) == kIOReturnSuccess;
  UMA_HISTOGRAM_BOOLEAN("Power.Mac.AppleSMCOpened", service_opened);
  if (!service_opened)
    return;
  impl_ = Impl::Create(base::mac::ScopedIOObject<io_object_t>(connect));
}

void PowerMetricsProvider::OnRecordingDisabled() {
  impl_.reset();
}
