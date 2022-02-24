// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "chromeos/services/bluetooth_config/public/cpp/device_image_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {
class DeviceImageInfo;
}  // namespace bluetooth_config
}  // namespace chromeos

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

class AccountKeyFilter;

using CheckAccountKeysCallback =
    base::OnceCallback<void(absl::optional<PairingMetadata>)>;
using DeviceMetadataCallback = base::OnceCallback<void(DeviceMetadata*, bool)>;
using ValidModelIdCallback = base::OnceCallback<void(bool)>;

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FastPairRepository {
 public:
  static FastPairRepository* Get();

  FastPairRepository();
  virtual ~FastPairRepository();

  // Returns the DeviceMetadata for a given |hex_model_id| to the provided
  // |callback|, if available.
  virtual void GetDeviceMetadata(const std::string& hex_model_id,
                                 DeviceMetadataCallback callback) = 0;

  // Checks all account keys associated with the user's account against the
  // given filter.  If a match is found, metadata for the associated device will
  // be returned through the callback.
  virtual void CheckAccountKeys(const AccountKeyFilter& account_key_filter,
                                CheckAccountKeysCallback callback) = 0;

  // Stores the given |account_key| for a |device| on the server.
  virtual void AssociateAccountKey(scoped_refptr<Device> device,
                                   const std::vector<uint8_t>& account_key) = 0;

  // Deletes the associated data for a given |device|.
  // Returns true if a delete will be processed for this device, false
  // otherwise.
  virtual bool DeleteAssociatedDevice(
      const device::BluetoothDevice* device) = 0;

  // Fetches the |device| images and a record of the device ID -> model ID
  // mapping to memory.
  virtual void FetchDeviceImages(scoped_refptr<Device> device) = 0;

  // Persists the images and device ID belonging to |device| to
  // disk, if model ID is not already persisted.
  virtual bool PersistDeviceImages(scoped_refptr<Device> device) = 0;

  // Evicts the images and device ID belonging to |device| from
  // disk, if model ID is not in use by other device IDs.
  virtual bool EvictDeviceImages(const device::BluetoothDevice* device) = 0;

  // Returns device images belonging to |device_id|, if found.
  virtual absl::optional<chromeos::bluetooth_config::DeviceImageInfo>
  GetImagesForDevice(const std::string& device_id) = 0;

 protected:
  static void SetInstance(FastPairRepository* instance);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
