// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/bluetooth_utility.h"

#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>
#include <IOKit/IOKitLib.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"

namespace bluetooth_utility {

BluetoothAvailability GetBluetoothAvailability() {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> matching_dict(
      IOServiceMatching("IOBluetoothHCIController"));
  if (!matching_dict)
    return BLUETOOTH_AVAILABILITY_ERROR;

  // IOServiceGetMatchingServices takes ownership of matching_dict.
  io_iterator_t iter;
  int kr = IOServiceGetMatchingServices(
      kIOMasterPortDefault, matching_dict.release(), &iter);
  if (kr != KERN_SUCCESS)
    return BLUETOOTH_NOT_AVAILABLE;
  base::mac::ScopedIOObject<io_iterator_t> scoped_iter(iter);

  int bluetooth_available = false;
  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(scoped_iter.get())), device) {
    bluetooth_available = true;

    CFMutableDictionaryRef dict;
    kr = IORegistryEntryCreateCFProperties(
        device, &dict, kCFAllocatorDefault, kNilOptions);
    if (kr != KERN_SUCCESS)
      continue;
    base::ScopedCFTypeRef<CFMutableDictionaryRef> scoped_dict(dict);

    NSDictionary* objc_dict = base::mac::CFToNSCast(scoped_dict.get());
    NSNumber* lmp_version =
        base::mac::ObjCCast<NSNumber>(objc_dict[@"LMPVersion"]);
    if (!lmp_version)
      continue;

    // The LMP version is too low to support Bluetooth LE.
    if ([lmp_version intValue] < 6)
      continue;

    NSData* data =
        base::mac::ObjCCast<NSData>(objc_dict[@"HCISupportedFeatures"]);

    NSUInteger supported_features_index = 4;
    NSUInteger length = [data length];
    if (length < supported_features_index + 1)
      continue;

    // The bytes are indexed in reverse order.
    NSUInteger index = length - supported_features_index - 1;

    const unsigned char* bytes =
        static_cast<const unsigned char*>([data bytes]);
    const unsigned char byte = bytes[index];
    bool le_supported = byte & kBluetoothFeatureLESupportedController;
    if (le_supported)
      return BLUETOOTH_AVAILABLE_WITH_LE;
  }

  return bluetooth_available ? BLUETOOTH_AVAILABLE_WITHOUT_LE
                             : BLUETOOTH_AVAILABILITY_ERROR;
}

}  // namespace bluetooth_utility
