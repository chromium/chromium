// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
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

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FakeFastPairRepository : public FastPairRepository {
 public:
  FakeFastPairRepository();
  FakeFastPairRepository(const FakeFastPairRepository&) = delete;
  FakeFastPairRepository& operator=(const FakeFastPairRepository&) = delete;
  ~FakeFastPairRepository() override;

  void SetFakeMetadata(const std::string& hex_model_id,
                       nearby::fastpair::Device metadata,
                       gfx::Image image = gfx::Image());

  void ClearFakeMetadata(const std::string& hex_model_id);

  void SetCheckAccountKeysResult(absl::optional<PairingMetadata> result);

  bool HasKeyForDevice(const std::string& mac_address);

  void set_is_network_connected(bool is_connected) {
    is_network_connected_ = is_connected;
  }

  // FastPairRepository::
  void GetDeviceMetadata(const std::string& hex_model_id,
                         DeviceMetadataCallback callback) override;
  void CheckAccountKeys(const AccountKeyFilter& account_key_filter,
                        CheckAccountKeysCallback callback) override;
  void AssociateAccountKey(scoped_refptr<Device> device,
                           const std::vector<uint8_t>& account_key) override;
  bool DeleteAssociatedDevice(const device::BluetoothDevice* device) override;
  void FetchDeviceImages(scoped_refptr<Device> device) override;
  bool PersistDeviceImages(scoped_refptr<Device> device) override;
  bool EvictDeviceImages(const device::BluetoothDevice* device) override;
  absl::optional<chromeos::bluetooth_config::DeviceImageInfo>
  GetImagesForDevice(const std::string& device_id) override;

 private:
  static void SetInstance(FastPairRepository* instance);

  bool is_network_connected_ = true;
  base::flat_map<std::string, std::unique_ptr<DeviceMetadata>> data_;
  base::flat_map<std::string, std::vector<uint8_t>> saved_account_keys_;
  absl::optional<PairingMetadata> check_account_key_result_;
  base::WeakPtrFactory<FakeFastPairRepository> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_
