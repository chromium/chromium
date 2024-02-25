// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"

namespace base {
namespace {

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is nullptr or it could not be converted to SInt64.
std::optional<SInt64> GetValueAsSInt64(CFDictionaryRef description,
                                       CFStringRef key) {
  CFNumberRef number_ref =
      base::apple::GetValueFromDictionary<CFNumberRef>(description, key);

  SInt64 value;
  if (number_ref && CFNumberGetValue(number_ref, kCFNumberSInt64Type, &value))
    return value;

  return std::nullopt;
}

std::optional<bool> GetValueAsBoolean(CFDictionaryRef description,
                                      CFStringRef key) {
  CFBooleanRef boolean =
      base::apple::GetValueFromDictionary<CFBooleanRef>(description, key);
  if (!boolean)
    return std::nullopt;
  return CFBooleanGetValue(boolean);
}

}  // namespace

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
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPMPowerSource")));
  if (!service) {
    // Macs without a battery don't necessarily provide the IOPMPowerSource
    // service (e.g. test bots). Don't report this as an error.
    return MakeBatteryState(/* battery_details=*/{});
  }

  apple::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result =
      IORegistryEntryCreateCFProperties(service.get(), dict.InitializeInto(),
                                        /*allocator=*/nullptr, /*options=*/0);

  if (result != KERN_SUCCESS) {
    // Failing to retrieve the dictionary is unexpected.
    return std::nullopt;
  }

  std::optional<bool> battery_installed =
      GetValueAsBoolean(dict.get(), CFSTR("BatteryInstalled"));
  if (!battery_installed.has_value()) {
    // Failing to access the BatteryInstalled property is unexpected.
    return std::nullopt;
  }

  if (!battery_installed.value()) {
    // BatteryInstalled == false means that there is no battery.
    return MakeBatteryState(/* battery_details=*/{});
  }

  std::optional<bool> external_connected =
      GetValueAsBoolean(dict.get(), CFSTR("ExternalConnected"));
  if (!external_connected.has_value()) {
    // Failing to access the ExternalConnected property is unexpected.
    return std::nullopt;
  }

  std::optional<SInt64> current_capacity =
      GetValueAsSInt64(dict.get(), CFSTR("AppleRawCurrentCapacity"));
  if (!current_capacity.has_value()) {
    return std::nullopt;
  }

  std::optional<SInt64> max_capacity =
      GetValueAsSInt64(dict.get(), CFSTR("AppleRawMaxCapacity"));
  if (!max_capacity.has_value()) {
    return std::nullopt;
  }

  std::optional<SInt64> voltage_mv =
      GetValueAsSInt64(dict.get(), CFSTR(kIOPSVoltageKey));
  if (!voltage_mv.has_value()) {
    return std::nullopt;
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
