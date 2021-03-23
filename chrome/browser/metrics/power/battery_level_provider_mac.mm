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

  void GetBatteryState(
      base::OnceCallback<void(const BatteryState&)> callback) override {
    std::vector<BatteryInterface> battery_interfaces =
        GetBatteryInterfaceList();
    std::move(callback).Run(
        BatteryLevelProvider::MakeBatteryState(battery_interfaces));
  }

 private:
  static std::vector<BatteryInterface> GetBatteryInterfaceList();

  static BatteryLevelProvider::BatteryInterface GetInterface(
      CFDictionaryRef description);
};

std::unique_ptr<BatteryLevelProvider> BatteryLevelProvider::Create() {
  return std::make_unique<BatteryLevelProviderMac>();
}

BatteryLevelProvider::BatteryInterface BatteryLevelProviderMac::GetInterface(
    CFDictionaryRef description) {
  base::Optional<bool> external_connected =
      GetValueAsBoolean(description, CFSTR("ExternalConnected"));
  if (!external_connected.has_value())
    return BatteryInterface(true);
  bool is_connected = *external_connected;

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
      GetValueAsSInt64(description, capacity_key);
  base::Optional<SInt64> max_capacity =
      GetValueAsSInt64(description, max_capacity_key);
  if (!current_capacity.has_value() || !max_capacity.has_value())
    return BatteryInterface(true);
  return BatteryInterface({is_connected, *current_capacity, *max_capacity});
}

std::vector<BatteryLevelProvider::BatteryInterface>
BatteryLevelProviderMac::GetBatteryInterfaceList() {
  // Retrieve the IOPMPowerSource service.
  const base::mac::ScopedIOObject<io_service_t> service(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPMPowerSource")));
  if (service == IO_OBJECT_NULL)
    return {};

  // Gather a dictionary containing the power information.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result = IORegistryEntryCreateCFProperties(
      service.get(), dict.InitializeInto(), 0, 0);

  std::vector<BatteryInterface> interfaces;
  // Retrieving dictionary failed. Cannot proceed.
  if (result != KERN_SUCCESS) {
    interfaces.push_back(BatteryInterface(false));
  } else {
    interfaces.push_back(GetInterface(dict));
  }
  return interfaces;
}
