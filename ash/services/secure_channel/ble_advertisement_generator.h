// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_
#define ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_

#include <memory>

#include "ash/services/secure_channel/foreground_eid_generator.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/components/multidevice/remote_device_ref.h"

namespace chromeos {
namespace secure_channel {
class SecureChannelBluetoothHelperImplTest;
}
}  // namespace chromeos

namespace ash::secure_channel {

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
  friend class chromeos::secure_channel::SecureChannelBluetoothHelperImplTest;

  static BleAdvertisementGenerator* instance_;

  // TODO(dcheng): Update this to follow the standard factory pattern.
  static void SetInstanceForTesting(BleAdvertisementGenerator* test_generator);

  void SetEidGeneratorForTesting(
      std::unique_ptr<ForegroundEidGenerator> test_eid_generator);

  std::unique_ptr<ForegroundEidGenerator> eid_generator_;
};

}  // namespace ash::secure_channel

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos::secure_channel {
using ::ash::secure_channel::BleAdvertisementGenerator;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_
