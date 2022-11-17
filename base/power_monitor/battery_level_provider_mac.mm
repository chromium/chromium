// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"

namespace base {
namespace {

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is nullptr or it could not be converted to SInt64.
absl::optional<SInt64> GetValueAsSInt64(CFDictionaryRef description,
                                        CFStringRef key) {
  CFNumberRef number_ref =
      base::mac::GetValueFromDictionary<CFNumberRef>(description, key);

  SInt64 value;
  if (number_ref && CFNumberGetValue(number_ref, kCFNumberSInt64Type, &value))
    return value;

  return absl::nullopt;
}

absl::optional<bool> GetValueAsBoolean(CFDictionaryRef description,
                                       CFStringRef key) {
  CFBooleanRef boolean =
      base::mac::GetValueFromDictionary<CFBooleanRef>(description, key);
  if (!boolean)
    return absl::nullopt;
  return CFBooleanGetValue(boolean);
}

}  // namespace

class BatteryLevelProviderMac : public BatteryLevelProvider {
 public:
  BatteryLevelProviderMac() = default;
  ~BatteryLevelProviderMac() override = default;

  void GetBatteryState(
      base::OnceCallback<void(const absl::optional<BatteryState>&)> callback)
      override {
    std::move(callback).Run(GetBatteryStateImpl());
  }

 private:
  absl::optional<BatteryState> GetBatteryStateImpl();
};

std::unique_ptr<BatteryLevelProvider> BatteryLevelProvider::Create() {
  return std::make_unique<BatteryLevelProviderMac>();
}

absl::optional<BatteryLevelProviderMac::BatteryState>
BatteryLevelProviderMac::GetBatteryStateImpl() {
  const base::mac::ScopedIOObject<io_service_t> service(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPMPowerSource")));
  if (service == IO_OBJECT_NULL) {
    // Macs without a battery don't necessarily provide the IOPMPowerSource
    // service (e.g. test bots). Don't report this as an error.
    return MakeBatteryState(/* battery_details=*/{});
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result = IORegistryEntryCreateCFProperties(
      service.get(), dict.InitializeInto(), 0, 0);

  if (result != KERN_SUCCESS) {
    // Failing to retrieve the dictionary is unexpected.
    return absl::nullopt;
  }

  absl::optional<bool> battery_installed =
      GetValueAsBoolean(dict, CFSTR("BatteryInstalled"));
  if (!battery_installed.has_value()) {
    // Failing to access the BatteryInstalled property is unexpected.
    return absl::nullopt;
  }

  if (!battery_installed.value()) {
    // BatteryInstalled == false means that there is no battery.
    return MakeBatteryState(/* battery_details=*/{});
  }

  absl::optional<bool> external_connected =
      GetValueAsBoolean(dict, CFSTR("ExternalConnected"));
  if (!external_connected.has_value()) {
    // Failing to access the ExternalConnected property is unexpected.
    return absl::nullopt;
  }

  CFStringRef capacity_key;
  CFStringRef max_capacity_key;

  // Use the correct capacity keys depending on macOS version.
  if (@available(macOS 10.14.0, *)) {
    capacity_key = CFSTR("AppleRawCurrentCapacity");
    max_capacity_key = CFSTR("AppleRawMaxCapacity");
  } else {
    capacity_key = CFSTR("CurrentCapacity");
    max_capacity_key = CFSTR("RawMaxCapacity");
  }

  absl::optional<SInt64> current_capacity =
      GetValueAsSInt64(dict, capacity_key);
  if (!current_capacity.has_value()) {
    return absl::nullopt;
  }

  absl::optional<SInt64> max_capacity =
      GetValueAsSInt64(dict, max_capacity_key);
  if (!max_capacity.has_value()) {
    return absl::nullopt;
  }

  absl::optional<SInt64> voltage_mv =
      GetValueAsSInt64(dict, CFSTR(kIOPSVoltageKey));
  if (!voltage_mv.has_value()) {
    return absl::nullopt;
  }

  DCHECK_GE(*current_capacity, 0);
  DCHECK_GE(*max_capacity, 0);
  DCHECK_GE(*voltage_mv, 0);

  return MakeBatteryState({BatteryDetails{
      .is_external_power_connected = external_connected.value(),
      .current_capacity = static_cast<uint64_t>(current_capacity.value()),
      .full_charged_capacity = static_cast<uint64_t>(max_capacity.value()),
      .voltage_mv = static_cast<uint64_t>(voltage_mv.value()),
      .charge_unit = BatteryLevelUnit::kMAh}});
}

}  // namespace base
