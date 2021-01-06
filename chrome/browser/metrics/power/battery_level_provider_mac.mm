// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/battery_level_provider.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"

namespace {

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is nullptr or it could not be converted to SInt64.
base::Optional<SInt64> GetValueAsSInt64(CFDictionaryRef description,
                                        CFStringRef key) {
  CFNumberRef number_ref =
      base::mac::GetValueFromDictionary<CFNumberRef>(description, key);

  SInt64 value;
  if (number_ref && CFNumberGetValue(number_ref, kCFNumberSInt64Type, &value))
    return value;

  return base::nullopt;
}

base::Optional<bool> GetValueAsBoolean(CFDictionaryRef description,
                                       CFStringRef key) {
  CFBooleanRef boolean =
      base::mac::GetValueFromDictionary<CFBooleanRef>(description, key);
  if (!boolean)
    return base::nullopt;
  return CFBooleanGetValue(boolean);
}

}  // namespace

class BatteryLevelProviderMac : public BatteryLevelProvider {
 public:
  BatteryLevelProviderMac() = default;
  ~BatteryLevelProviderMac() override = default;

  base::Optional<BatteryState> GetBatteryState() override;
};

std::unique_ptr<BatteryLevelProvider> BatteryLevelProvider::Create() {
  return std::make_unique<BatteryLevelProviderMac>();
}

base::Optional<BatteryLevelProvider::BatteryState>
BatteryLevelProviderMac::GetBatteryState() {
  const base::TimeTicks capture_time = base::TimeTicks::Now();

  // Retrieve the IOPMPowerSource service.
  const base::mac::ScopedIOObject<io_service_t> service(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPMPowerSource")));

  // Gather a dictionary containing the power information.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result = IORegistryEntryCreateCFProperties(
      service.get(), dict.InitializeInto(), 0, 0);

  // Retrieving dictionary failed. Cannot proceed.
  if (result != KERN_SUCCESS)
    return base::nullopt;

  base::Optional<bool> external_connected =
      GetValueAsBoolean(dict, CFSTR("ExternalConnected"));
  // Value was not available.
  if (!external_connected.has_value())
    return base::nullopt;

  CFStringRef capacity_key;
  CFStringRef max_capacity_key;

  // Use the correct key depending on macOS version.
  if (@available(macOS 10.14.0, *)) {
    capacity_key = CFSTR("AppleRawCurrentCapacity");
    max_capacity_key = CFSTR("AppleRawMaxCapacity");
  } else {
    capacity_key = CFSTR("CurrentCapacity");
    max_capacity_key = CFSTR("RawMaxCapacity");
  }

  // Extract the information from the dictionary.
  base::Optional<SInt64> current_capacity =
      GetValueAsSInt64(dict, capacity_key);
  base::Optional<SInt64> max_capacity =
      GetValueAsSInt64(dict, max_capacity_key);

  // If any of the values were not available.
  if (!current_capacity.has_value() || !max_capacity.has_value())
    return base::nullopt;

  // Avoid invalid division.
  if (*max_capacity == 0)
    return base::nullopt;

  // |ratio| is the result of dividing |current_capacity| by |max_capacity|.
  double charge_level = static_cast<double>(current_capacity.value()) /
                        static_cast<double>(max_capacity.value());

  return BatteryState{charge_level, !(*external_connected), capture_time};
}
