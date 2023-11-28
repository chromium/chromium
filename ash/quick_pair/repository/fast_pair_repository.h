// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"

namespace chromeos {
namespace bluetooth_config {
class DeviceImageInfo;
}  // namespace bluetooth_config
}  // namespace chromeos

namespace ash {
namespace quick_pair {

class AccountKeyFilter;

using CheckAccountKeysCallback =
    base::OnceCallback<void(std::optional<PairingMetadata>)>;
using DeviceMetadataCallback = base::OnceCallback<void(DeviceMetadata*, bool)>;
using ValidModelIdCallback = base::OnceCallback<void(bool)>;
using CheckOptInStatusCallback =
    base::OnceCallback<void(nearby::fastpair::OptInStatus)>;
using UpdateOptInStatusCallback = base::OnceCallback<void(bool)>;
using DeleteAssociatedDeviceCallback = base::OnceCallback<void(bool)>;
using DeleteAssociatedDeviceByAccountKeyCallback =
    base::OnceCallback<void(bool)>;
using GetSavedDevicesCallback =
    base::OnceCallback<void(nearby::fastpair::OptInStatus,
                            std::vector<nearby::fastpair::FastPairDevice>)>;
using IsDeviceSavedToAccountCallback = base::OnceCallback<void(bool)>;

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FastPairRepository {
 public:
  static FastPairRepository* Get();

  // Computes and returns the SHA256 of the concatenation of the given
  // |account_key| and |mac_address|.
  static std::string GenerateSha256OfAccountKeyAndMacAddress(
      const std::string& account_key,
      const std::string& mac_address);

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

  // Checks account keys saved to the device registry for a match to
  // |account_key|. Return true if a match is found and false otherwise.
  virtual bool IsAccountKeyPairedLocally(
      const std::vector<uint8_t>& account_key) = 0;

  // Stores the given |account_key| for a |device| on the Footprints server.
  virtual void WriteAccountAssociationToFootprints(
      scoped_refptr<Device> device,
      const std::vector<uint8_t>& account_key) = 0;

  // Stores the account_key for a |device| locally in the SavedDeviceRegistry,
  // skipping the step where we send a request to the server. The account key
  // should be stored in the additional data field of the device, fails
  // otherwise.
  virtual bool WriteAccountAssociationToLocalRegistry(
      scoped_refptr<Device> device) = 0;

  // Deletes the associated data for device with a given |mac_address|.
  // Returns true if a delete will be processed for this device, false
  // otherwise.
  virtual void DeleteAssociatedDevice(
      const std::string& mac_address,
      DeleteAssociatedDeviceCallback callback) = 0;

  // Updates the display name of the device saved on the Footprints server. The
  // function will first check the cache for the device, if the device is not
  // found in the cache and |cache_may_be_stale| is set to true, it will trigger
  // a server call to refresh the cache.
  virtual void UpdateAssociatedDeviceFootprintsName(
      const std::string& mac_address,
      const std::string& nickname,
      bool cache_may_be_stale) = 0;

  // Deletes the associated data for a given |account_key|.
  // Runs true if a delete is successful for this account key, false
  // otherwise on |callback|.
  virtual void DeleteAssociatedDeviceByAccountKey(
      const std::vector<uint8_t>& account_key,
      DeleteAssociatedDeviceByAccountKeyCallback callback) = 0;

  // Fetches the |device| images and a record of the device ID -> model ID
  // mapping to memory.
  virtual void FetchDeviceImages(scoped_refptr<Device> device) = 0;

  // Fetches the |device| display_name from the cache, this is only called
  // during the subsequent pairing scenario and is called after the account_key
  // filter has been matched to an account_key. Therefore the account_key and
  // associated display_name should already be in the cache so no need for a
  // server call.
  virtual std::optional<std::string> GetDeviceDisplayNameFromCache(
      std::vector<uint8_t> account_key) = 0;

  // Persists the images and device ID belonging to |device| to
  // disk, if model ID is not already persisted.
  virtual bool PersistDeviceImages(scoped_refptr<Device> device) = 0;

  // Evicts the images and mac address record for |mac_address| from
  // disk, if model ID is not in use by other mac addresses.
  virtual bool EvictDeviceImages(const std::string& mac_address) = 0;

  // Returns device images belonging to |mac_address|, if found.
  virtual std::optional<bluetooth_config::DeviceImageInfo> GetImagesForDevice(
      const std::string& mac_address) = 0;

  // Fetches the opt in status from Footprints to determine the status for
  // saving a user's devices to their account, which is synced all across a
  // user's devices.
  virtual void CheckOptInStatus(CheckOptInStatusCallback callback) = 0;

  // Updates the opt in status for saving a user's devices to their account,
  // synced across all of a user's devices, based on |opt_in_status|.
  // If |opt_in_status| reflects the user being opted out, then all devices
  // saved to their account are removed. The success or failure of updating
  // the opt in status is reflected in running |callback|.
  virtual void UpdateOptInStatus(nearby::fastpair::OptInStatus opt_in_status,
                                 UpdateOptInStatusCallback callback) = 0;

  // Gets a list of devices saved to the user's account and the user's opt in
  // status for saving future devices to their account.
  virtual void GetSavedDevices(GetSavedDevicesCallback callback) = 0;

  // Checks if a device with an address |mac_address| is already saved to
  // the user's account by cross referencing the |mac_address| with any
  // associated account keys.
  virtual void IsDeviceSavedToAccount(
      const std::string& mac_address,
      IsDeviceSavedToAccountCallback callback) = 0;

  static void SetInstance(FastPairRepository* instance);
  static void SetInstanceForTesting(FastPairRepository* instance);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
