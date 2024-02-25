// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"

namespace ash::bluetooth_config {
class DeviceImageInfo;
}

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

  void SetCheckAccountKeysResult(std::optional<PairingMetadata> result);

  void set_is_account_key_paired_locally(bool is_account_key_paired_locally) {
    is_account_key_paired_locally_ = is_account_key_paired_locally;
  }

  bool HasKeyForDevice(const std::string& mac_address);

  bool HasNameForDevice(const std::string& mac_address);

  void set_is_network_connected(bool is_connected) {
    is_network_connected_ = is_connected;
  }

  void SetOptInStatus(nearby::fastpair::OptInStatus status);
  nearby::fastpair::OptInStatus GetOptInStatus();

  void SetSavedDevices(nearby::fastpair::OptInStatus status,
                       std::vector<nearby::fastpair::FastPairDevice> devices);

  void SaveMacAddressToAccount(const std::string& mac_address);

  // FastPairRepository::
  void GetDeviceMetadata(const std::string& hex_model_id,
                         DeviceMetadataCallback callback) override;
  void CheckAccountKeys(const AccountKeyFilter& account_key_filter,
                        CheckAccountKeysCallback callback) override;
  void WriteAccountAssociationToFootprints(
      scoped_refptr<Device> device,
      const std::vector<uint8_t>& account_key) override;
  bool WriteAccountAssociationToLocalRegistry(
      scoped_refptr<Device> device) override;
  void DeleteAssociatedDevice(const std::string& mac_address,
                              DeleteAssociatedDeviceCallback callback) override;
  void UpdateAssociatedDeviceFootprintsName(const std::string& mac_address,
                                            const std::string& display_name,
                                            bool cache_may_be_stale) override;

  void FetchDeviceImages(scoped_refptr<Device> device) override;
  std::optional<std::string> GetDeviceDisplayNameFromCache(
      std::vector<uint8_t> account_key) override;
  bool PersistDeviceImages(scoped_refptr<Device> device) override;
  bool EvictDeviceImages(const std::string& mac_address) override;
  std::optional<bluetooth_config::DeviceImageInfo> GetImagesForDevice(
      const std::string& mac_address) override;
  void CheckOptInStatus(CheckOptInStatusCallback callback) override;
  void UpdateOptInStatus(nearby::fastpair::OptInStatus opt_in_status,
                         UpdateOptInStatusCallback callback) override;
  void DeleteAssociatedDeviceByAccountKey(
      const std::vector<uint8_t>& account_key,
      DeleteAssociatedDeviceByAccountKeyCallback callback) override;
  void GetSavedDevices(GetSavedDevicesCallback callback) override;
  bool IsAccountKeyPairedLocally(
      const std::vector<uint8_t>& account_key) override;
  void IsDeviceSavedToAccount(const std::string& mac_address,
                              IsDeviceSavedToAccountCallback callback) override;

  // `SetIsDeviceSavedToAccountCallbackDelay` and
  // `TriggerIsDeviceSavedToAccountCallback` are used together to control when
  // the callback is triggered.
  void TriggerIsDeviceSavedToAccountCallback();
  void SetIsDeviceSavedToAccountCallbackDelayed(bool is_delayed) {
    saved_to_account_callback_is_delayed_ = is_delayed;
  }

 private:
  static void SetInstance(FastPairRepository* instance);

  IsDeviceSavedToAccountCallback saved_to_account_callback_;
  bool saved_to_account_callback_is_delayed_ = false;
  nearby::fastpair::OptInStatus status_ =
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN;
  std::vector<nearby::fastpair::FastPairDevice> devices_;
  bool is_network_connected_ = true;
  bool is_account_key_paired_locally_ = true;
  base::flat_set<std::string> saved_mac_addresses_;

  // The key for 'data_' is ASCII model ids.
  base::flat_map<std::string, std::unique_ptr<DeviceMetadata>> data_;

  // The key for 'saved_accout_keys_' is the device's classic address.
  base::flat_map<std::string, std::vector<uint8_t>> saved_account_keys_;

  // The key for 'saved_display_names_' is the device's classic address.
  base::flat_map<std::string, std::string> saved_display_names_;

  std::optional<PairingMetadata> check_account_keys_result_;
  base::WeakPtrFactory<FakeFastPairRepository> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_
