// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"

namespace base {

class BatteryLevelProviderMac : public BatteryLevelProvider {
 public:
  BatteryLevelProviderMac() = default;
  ~BatteryLevelProviderMac() override = default;

  void GetBatteryState(
      base::OnceCallback<void(const std::optional<BatteryState>&)> callback)
      override {
    std::move(callback).Run(GetBatteryStateImpl());
  }

 private:
  std::optional<BatteryState> GetBatteryStateImpl();
};

std::unique_ptr<BatteryLevelProvider> BatteryLevelProvider::Create() {
  return std::make_unique<BatteryLevelProviderMac>();
}

std::optional<BatteryLevelProviderMac::BatteryState>
BatteryLevelProviderMac::GetBatteryStateImpl() {
  const base::mac::ScopedIOObject<io_service_t> service(
      IOServiceGetMatchingService(kIOMainPortDefault,
                                  IOServiceMatching("IOPMPowerSource")));
  if (!service) {
    // Macs without a battery don't necessarily provide the IOPMPowerSource
    // service (e.g. test bots). Don't report this as an error.
    return MakeBatteryState(/* battery_details=*/{});
  }

  // Note that there is API to get battery info (IOPSCopyPowerSourcesInfo(),
  // IOPSCopyPowerSourcesList(), IOPSGetPowerSourceDescription()).
  // Unfortunately, that API is not useful:
  //
  // - Capacity values are reported in a 0-100% scale, not in absolute values
  // - Voltage values are often not present, in contradiction of the
  //   documentation for kIOPSVoltageKey
  //
  // Therefore, alas, go mucking around in the IORegistry.

  apple::ScopedCFTypeRef<CFMutableDictionaryRef> dict_cf;
  kern_return_t result =
      IORegistryEntryCreateCFProperties(service.get(), dict_cf.InitializeInto(),
                                        /*allocator=*/nullptr, /*options=*/0);
  NSDictionary* dict = base::apple::CFToNSPtrCast(dict_cf.get());
  if (result != KERN_SUCCESS || !dict) {
    // Failing to retrieve the dictionary is unexpected.
    return std::nullopt;
  }

  NSNumber* battery_installed =
      base::apple::ObjCCast<NSNumber>([dict objectForKey:@"BatteryInstalled"]);
  if (!battery_installed || !battery_installed.boolValue) {
    // Failing to access the BatteryInstalled property is unexpected.
    // BatteryInstalled == false means that there is no battery.
    return std::nullopt;
  }

  NSNumber* external_connected =
      base::apple::ObjCCast<NSNumber>([dict objectForKey:@"ExternalConnected"]);
  if (!external_connected) {
    // Failing to access the ExternalConnected property is unexpected.
    return std::nullopt;
  }

  NSNumber* current_capacity;
  NSNumber* full_charged_capacity;
  if (@available(macOS 27, *)) {
    if (NSDictionary* battery_data = base::apple::ObjCCast<NSDictionary>(
            [dict objectForKey:@"BatteryData"])) {
      current_capacity = base::apple::ObjCCast<NSNumber>(
          [battery_data objectForKey:@"RemainingCapacity"]);
      full_charged_capacity = base::apple::ObjCCast<NSNumber>(
          [battery_data objectForKey:@"FullChargeCapacity"]);
    }
  } else {
    current_capacity = base::apple::ObjCCast<NSNumber>(
        [dict objectForKey:@"AppleRawCurrentCapacity"]);
    full_charged_capacity = base::apple::ObjCCast<NSNumber>(
        [dict objectForKey:@"AppleRawMaxCapacity"]);
  }
  if (!current_capacity || !full_charged_capacity) {
    return std::nullopt;
  }

  NSNumber* voltage_mv =
      base::apple::ObjCCast<NSNumber>([dict objectForKey:@"Voltage"]);
  if (!voltage_mv) {
    return std::nullopt;
  }

  // Paranoia; these values should be positive integers but DCHECK in case they
  // are not.
  DCHECK_GE(current_capacity.longValue, 0);
  DCHECK(!CFNumberIsFloatType(base::apple::NSToCFPtrCast(current_capacity)));
  DCHECK_GE(full_charged_capacity.longValue, 0);
  DCHECK(
      !CFNumberIsFloatType(base::apple::NSToCFPtrCast(full_charged_capacity)));
  DCHECK_GE(voltage_mv.longValue, 0);
  DCHECK(!CFNumberIsFloatType(base::apple::NSToCFPtrCast(voltage_mv)));

  return MakeBatteryState({BatteryDetails{
      .is_external_power_connected =
          static_cast<bool>(external_connected.boolValue),
      .current_capacity = current_capacity.unsignedLongValue,
      .full_charged_capacity = full_charged_capacity.unsignedLongValue,
      .voltage_mv = voltage_mv.unsignedLongValue,
      .charge_unit = BatteryLevelUnit::kMAh}});
}

}  // namespace base
