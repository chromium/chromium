// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_BLUETOOTH_UTILITY_H_
#define CHROME_BROWSER_MAC_BLUETOOTH_UTILITY_H_

namespace bluetooth_utility {

// The enum is used in a histogram, so the values must not change.
enum BluetoothAvailability {
  BLUETOOTH_AVAILABILITY_ERROR = 0,  // Error determining availability.
  BLUETOOTH_NOT_AVAILABLE = 1,
  BLUETOOTH_AVAILABLE_WITHOUT_LE = 2,
  BLUETOOTH_AVAILABLE_WITH_LE = 3,

  // On OSX 10.6, if the Link Manager Protocol version supports Low Energy,
  // there is no further indication of whether Low Energy is supported.
  BLUETOOTH_AVAILABLE_LE_UNKNOWN = 4,
  BLUETOOTH_NOT_SUPPORTED = 5,
  kMaxValue = BLUETOOTH_NOT_SUPPORTED
};

// Returns the bluetooth availability of the system's hardware.
BluetoothAvailability GetBluetoothAvailability();

}  // namespace bluetooth_utility

#endif  // CHROME_BROWSER_MAC_BLUETOOTH_UTILITY_H_
