// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_
#define ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_

#include <memory>

// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/components/multidevice/remote_device_ref.h"

namespace ash::secure_channel {

class ForegroundEidGenerator;
struct DataWithTimestamp;

// Generates advertisements for the ProximityAuth BLE advertisement scheme.
class BleAdvertisementGenerator {
 public:
  // Generates an advertisement from the current device to |remote_device|. The
  // generated advertisement should be used immediately since it is based on the
  // current timestamp.
  static std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisement(
      multidevice::RemoteDeviceRef remote_device,
      const std::string& local_device_public_key);

  BleAdvertisementGenerator(const BleAdvertisementGenerator&) = delete;
  BleAdvertisementGenerator& operator=(const BleAdvertisementGenerator&) =
      delete;

  virtual ~BleAdvertisementGenerator();

 protected:
  BleAdvertisementGenerator();

  virtual std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisementInternal(
      multidevice::RemoteDeviceRef remote_device,
      const std::string& local_device_public_key);

 private:
  friend class SecureChannelBleAdvertisementGeneratorTest;
  friend class SecureChannelBluetoothHelperImplTest;

  static BleAdvertisementGenerator* instance_;

  // TODO(dcheng): Update this to follow the standard factory pattern.
  static void SetInstanceForTesting(BleAdvertisementGenerator* test_generator);

  void SetEidGeneratorForTesting(
      std::unique_ptr<ForegroundEidGenerator> test_eid_generator);

  std::unique_ptr<ForegroundEidGenerator> eid_generator_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_
