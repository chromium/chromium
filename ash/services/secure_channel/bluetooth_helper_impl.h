// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_BLUETOOTH_HELPER_IMPL_H_
#define ASH_SERVICES_SECURE_CHANNEL_BLUETOOTH_HELPER_IMPL_H_

#include <memory>
#include <string>

#include "ash/services/secure_channel/bluetooth_helper.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/components/multidevice/remote_device_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::secure_channel {

class BackgroundEidGenerator;
class ForegroundEidGenerator;

// Concrete BluetoothHelper implementation.
class BluetoothHelperImpl : public BluetoothHelper {
 public:
  class Factory {
   public:
    static std::unique_ptr<BluetoothHelper> Create(
        multidevice::RemoteDeviceCache* remote_device_cache);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<BluetoothHelper> CreateInstance(
        multidevice::RemoteDeviceCache* remote_device_cache) = 0;

   private:
    static Factory* test_factory_;
  };

  BluetoothHelperImpl(const BluetoothHelperImpl&) = delete;
  BluetoothHelperImpl& operator=(const BluetoothHelperImpl&) = delete;

  ~BluetoothHelperImpl() override;

 private:
  friend class SecureChannelBluetoothHelperImplTest;

  explicit BluetoothHelperImpl(
      multidevice::RemoteDeviceCache* remote_device_cache);

  // BluetoothHelper:
  std::unique_ptr<DataWithTimestamp> GenerateForegroundAdvertisement(
      const DeviceIdPair& device_id_pair) override;
  absl::optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set) override;
  std::string GetBluetoothPublicAddress(const std::string& device_id) override;
  std::string ExpectedServiceDataToString(
      const DeviceIdPairSet& device_id_pair_set) override;

  absl::optional<BluetoothHelper::DeviceWithBackgroundBool>
  PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const std::string& local_device_id,
      const std::vector<std::string>& remote_device_ids);

  void SetTestDoubles(
      std::unique_ptr<BackgroundEidGenerator> background_eid_generator,
      std::unique_ptr<ForegroundEidGenerator> foreground_eid_generator);

  multidevice::RemoteDeviceCache* remote_device_cache_;
  std::unique_ptr<BackgroundEidGenerator> background_eid_generator_;
  std::unique_ptr<ForegroundEidGenerator> foreground_eid_generator_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_BLUETOOTH_HELPER_IMPL_H_
