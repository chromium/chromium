// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace chromeos {
namespace bluetooth_config {
class DeviceImageInfo;
}  // namespace bluetooth_config
}  // namespace chromeos

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace nearby {
namespace fastpair {
class UserReadDevicesResponse;
}  // namespace fastpair
}  // namespace nearby

namespace ash {
namespace quick_pair {

class DeviceAddressMap;
class DeviceImageStore;
class DeviceMetadataFetcher;
class FastPairImageDecoder;
class FootprintsFetcher;
class PendingWriteStore;
class SavedDeviceRegistry;

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FastPairRepositoryImpl : public FastPairRepository,
                               public NetworkStateHandlerObserver {
 public:
  FastPairRepositoryImpl();
  FastPairRepositoryImpl(
      scoped_refptr<device::BluetoothAdapter> adapter,
      std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher,
      std::unique_ptr<FootprintsFetcher> footprints_fetcher,
      std::unique_ptr<FastPairImageDecoder> image_decoder,
      std::unique_ptr<DeviceAddressMap> device_address_map,
      std::unique_ptr<DeviceImageStore> device_image_store,
      std::unique_ptr<SavedDeviceRegistry> saved_device_registry,
      std::unique_ptr<PendingWriteStore> pending_write_store);
  FastPairRepositoryImpl(const FastPairRepositoryImpl&) = delete;
  FastPairRepositoryImpl& operator=(const FastPairRepositoryImpl&) = delete;
  ~FastPairRepositoryImpl() override;

  // FastPairRepository:
  void GetDeviceMetadata(const std::string& hex_model_id,
                         DeviceMetadataCallback callback) override;
  void CheckAccountKeys(const AccountKeyFilter& account_key_filter,
                        CheckAccountKeysCallback callback) override;
  bool IsAccountKeyPairedLocally(
      const std::vector<uint8_t>& account_key) override;
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
  void DeleteAssociatedDeviceByAccountKey(
      const std::vector<uint8_t>& account_key,
      DeleteAssociatedDeviceByAccountKeyCallback callback) override;
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
  void GetSavedDevices(GetSavedDevicesCallback callback) override;
  void IsDeviceSavedToAccount(const std::string& mac_address,
                              IsDeviceSavedToAccountCallback callback) override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

 private:
  void CheckAccountKeysImpl(const AccountKeyFilter& account_key_filter,
                            CheckAccountKeysCallback callback,
                            bool allow_cache_refresh);
  void OnMetadataFetched(
      const std::string& normalized_model_id,
      DeviceMetadataCallback callback,
      std::optional<nearby::fastpair::GetObservedDeviceResponse> response,
      bool has_retryable_error);
  void OnImageDecoded(const std::string& normalized_model_id,
                      DeviceMetadataCallback callback,
                      nearby::fastpair::GetObservedDeviceResponse response,
                      gfx::Image image);
  void RetryCheckAccountKeys(
      const AccountKeyFilter& account_key_filter,
      CheckAccountKeysCallback callback,
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);
  void UpdateCacheAndRetryCheckAccountKeys(
      const AccountKeyFilter& account_key_filter,
      CheckAccountKeysCallback callback,
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);

  void UpdateCacheAndRetryChangeDisplayName(
      const std::string& mac_address,
      const std::string& display_name,
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);

  void UpdateUserDevicesCache(
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);
  void CompleteAccountKeyLookup(CheckAccountKeysCallback callback,
                                std::vector<uint8_t> account_key,
                                DeviceMetadata* device_metadata,
                                bool has_retryable_error);
  void WriteAccountAssociationToFootprintsWithMetadata(
      const std::string& hex_model_id,
      const std::string& mac_address,
      const std::optional<std::string>& display_name,
      const std::vector<uint8_t>& account_key,
      std::optional<Protocol> device_protocol,
      DeviceMetadata* metadata,
      bool has_retryable_error);
  void OnWriteAccountAssociationToFootprintsComplete(
      const std::string& mac_address,
      const std::vector<uint8_t>& account_key,
      std::optional<Protocol> device_protocol,
      bool success);
  void OnCheckOptInStatus(
      CheckOptInStatusCallback callback,
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);
  void OnUpdateOptInStatusComplete(UpdateOptInStatusCallback callback,
                                   bool success);
  // Fetches the |device_metadata| images to the DeviceImageStore for
  // |hex_model_id|.
  void CompleteFetchDeviceImages(const std::string& hex_model_id,
                                 DeviceMetadata* device_metadata,
                                 bool has_retryable_error);
  void OnDeleteAssociatedDevice(const std::string& mac_address,
                                DeleteAssociatedDeviceCallback callback,
                                bool success);
  void OnDeleteAssociatedDeviceByAccountKey(
      const std::vector<uint8_t>& account_key,
      DeleteAssociatedDeviceByAccountKeyCallback callback,
      bool footprints_removal_success);
  void RetryPendingDeletes(
      nearby::fastpair::OptInStatus status,
      std::vector<nearby::fastpair::FastPairDevice> devices);

  // Retries adding device fast pair information to the Footprints server in the
  // case that network connection isn't established when the Fast Pair
  // Repository attempts to write to Footprints.
  void RetryPendingWrites();
  void OnGetSavedDevices(
      GetSavedDevicesCallback callback,
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);

  // Iterates over the list of |user_devices| and checks if the given
  // |mac_address| matches any by comparing the
  // SHA256(concat (account key, mac address)).
  void CompleteIsDeviceSavedToAccount(
      const std::string& mac_address,
      IsDeviceSavedToAccountCallback callback,
      std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  scoped_refptr<device::BluetoothAdapter> adapter_;

  std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher_;
  std::unique_ptr<FootprintsFetcher> footprints_fetcher_;
  std::unique_ptr<FastPairImageDecoder> image_decoder_;
  std::unique_ptr<DeviceAddressMap> device_address_map_;
  std::unique_ptr<DeviceImageStore> device_image_store_;
  std::unique_ptr<SavedDeviceRegistry> saved_device_registry_;
  std::unique_ptr<PendingWriteStore> pending_write_store_;

  base::flat_map<std::string, std::unique_ptr<DeviceMetadata>> metadata_cache_;
  nearby::fastpair::UserReadDevicesResponse user_devices_cache_;
  base::Time footprints_last_updated_;
  base::Time retry_write_or_delete_last_attempted_;

  base::WeakPtrFactory<FastPairRepositoryImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_
