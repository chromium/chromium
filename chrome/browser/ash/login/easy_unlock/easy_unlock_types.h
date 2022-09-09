// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TYPES_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TYPES_H_

#include <string>
#include <vector>

namespace ash {

extern const char kEasyUnlockKeyMetaNameBluetoothAddress[];
extern const char kEasyUnlockKeyMetaNamePsk[];
extern const char kEasyUnlockKeyMetaNamePubKey[];
extern const char kEasyUnlockKeyMetaNameChallenge[];
extern const char kEasyUnlockKeyMetaNameWrappedSecret[];
extern const char kEasyUnlockKeyMetaNameSerializedBeaconSeeds[];
extern const char kEasyUnlockKeyMetaNameUnlockKey[];

// Device data that is stored with cryptohome keys.
struct EasyUnlockDeviceKeyData {
  EasyUnlockDeviceKeyData();
  EasyUnlockDeviceKeyData(const EasyUnlockDeviceKeyData&);
  ~EasyUnlockDeviceKeyData();

  // Bluetooth address of the remote device.
  std::string bluetooth_address;
  // Public key of the remote device.
  std::string public_key;
  // Key to establish a secure channel with the remote device.
  std::string psk;
  // Challenge bytes to be sent to the phone.
  std::string challenge;
  // Wrapped secret to mount cryptohome home.
  std::string wrapped_secret;
  // Serialized BeaconSeeds used to identify this device.
  std::string serialized_beacon_seeds;
  // True if the device is an Easy Unlock host, false if not (which implies
  // that it is the local device).
  bool unlock_key;
};
typedef std::vector<EasyUnlockDeviceKeyData> EasyUnlockDeviceKeyDataList;

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TYPES_H_
