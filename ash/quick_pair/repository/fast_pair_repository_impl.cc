// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository_impl.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_address_map.h"
#include "ash/quick_pair/repository/fast_pair/device_image_store.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder_impl.h"
#include "ash/quick_pair/repository/fast_pair/footprints_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/footprints_fetcher_impl.h"
#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"
#include "ash/quick_pair/repository/fast_pair/proto_conversions.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "components/cross_device/logging/logging.h"
#include "crypto/sha2.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace {

constexpr base::TimeDelta kOfflineRetryTimeout = base::Minutes(1);
constexpr base::TimeDelta kCacheInvalidationTime = base::Minutes(30);
// This forget pattern is defined in the Android codebase as FORGET_PREFIX_BYTE
// and FORGET_PREFIX_LENGTH_IN_BYTES. Currently, those values evaluate to the
// string of bytes defined below, which is used as the prefix for the sha256
// field of the device. This should be kept in sync with those values.
// http://google3/java/com/google/location/nearby/common/fastpair/footprints/FootprintsDeviceManager.java;l=65-75;rcl=482615113
const std::string kForgetPattern = "\xf0\xf0\xf0\xf0";

// For all intents and purposes, a device that has the "Forget pattern" is no
// longer associated to the user's account, and should be treated as removed.
bool DoesDeviceHaveForgetPattern(
    const nearby::fastpair::FastPairDevice& device) {
  // The device info is modified to have no account key upon removal from
  // Android Saved Devices, removal from CrOS Saved Devices, and forget from
  // CrOS Bluetooth Settings.
  if (!device.has_account_key() ||
      !device.has_sha256_account_key_public_address()) {
    return true;
  }

  // To match Android behavior, we check if the SHA256 of a device begins with
  // the Forget pattern, defined in Android Fast Pair code. When a device is
  // forgotten from Android Bluetooth Settings, the SHA256 hash is modified to
  // contain this pattern.
  return (device.sha256_account_key_public_address().compare(
              0, kForgetPattern.length(), kForgetPattern) == 0);
}

// Checks if the mac address of a FastPairDevice is the same as the given
// |mac_address| by checking if the SHA256 from the given |device| equals to
// SHA256(concat(account_key of |device|, |mac_address|)).
bool IsDeviceSha256Matched(const nearby::fastpair::FastPairDevice& device,
                           const std::string& mac_address) {
  if (DoesDeviceHaveForgetPattern(device)) {
    return false;
  }

  return device.sha256_account_key_public_address() ==
         ash::quick_pair::FastPairRepository::
             GenerateSha256OfAccountKeyAndMacAddress(device.account_key(),
                                                     mac_address);
}

}  // namespace

namespace ash {
namespace quick_pair {

FastPairRepositoryImpl::FastPairRepositoryImpl()
    : device_metadata_fetcher_(std::make_unique<DeviceMetadataFetcher>()),
      footprints_fetcher_(std::make_unique<FootprintsFetcherImpl>()),
      image_decoder_(std::make_unique<FastPairImageDecoderImpl>()),
      device_address_map_(std::make_unique<DeviceAddressMap>()),
      device_image_store_(
          std::make_unique<DeviceImageStore>(image_decoder_.get())),
      footprints_last_updated_(base::Time::UnixEpoch()) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &FastPairRepositoryImpl::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
  SetInstance(this);
}

void FastPairRepositoryImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  saved_device_registry_ = std::make_unique<SavedDeviceRegistry>(adapter_);
}

FastPairRepositoryImpl::FastPairRepositoryImpl(
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher,
    std::unique_ptr<FootprintsFetcher> footprints_fetcher,
    std::unique_ptr<FastPairImageDecoder> image_decoder,
    std::unique_ptr<DeviceAddressMap> device_address_map,
    std::unique_ptr<DeviceImageStore> device_image_store,
    std::unique_ptr<SavedDeviceRegistry> saved_device_registry,
    std::unique_ptr<PendingWriteStore> pending_write_store)
    : adapter_(adapter),
      device_metadata_fetcher_(std::move(device_metadata_fetcher)),
      footprints_fetcher_(std::move(footprints_fetcher)),
      image_decoder_(std::move(image_decoder)),
      device_address_map_(std::move(device_address_map)),
      device_image_store_(std::move(device_image_store)),
      saved_device_registry_(std::move(saved_device_registry)),
      pending_write_store_(std::move(pending_write_store)),
      footprints_last_updated_(base::Time::UnixEpoch()),
      retry_write_or_delete_last_attempted_(base::Time::UnixEpoch()) {}

FastPairRepositoryImpl::~FastPairRepositoryImpl() {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
  SetInstance(nullptr);
}

void FastPairRepositoryImpl::GetDeviceMetadata(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback) {
  std::string normalized_id = base::ToUpperASCII(hex_model_id);
  if (metadata_cache_.contains(normalized_id)) {
    CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Data already in cache.";
    RecordFastPairRepositoryCacheResult(/*success=*/true);
    std::move(callback).Run(metadata_cache_[normalized_id].get(),
                            /*has_retryable_error=*/false);
    return;
  }
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Not cached, fetching from web service.";
  RecordFastPairRepositoryCacheResult(/*success=*/false);
  device_metadata_fetcher_->LookupHexDeviceId(
      normalized_id, base::BindOnce(&FastPairRepositoryImpl::OnMetadataFetched,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    normalized_id, std::move(callback)));
}

void FastPairRepositoryImpl::OnMetadataFetched(
    const std::string& normalized_model_id,
    DeviceMetadataCallback callback,
    std::optional<nearby::fastpair::GetObservedDeviceResponse> response,
    bool has_retryable_error) {
  if (!response) {
    std::move(callback).Run(nullptr, has_retryable_error);
    return;
  }
  if (response->image().empty()) {
    metadata_cache_[normalized_model_id] =
        std::make_unique<DeviceMetadata>(std::move(*response), gfx::Image());
    std::move(callback).Run(metadata_cache_[normalized_model_id].get(),
                            /*has_retryable_error=*/false);
    return;
  }

  const std::string& string_data = response->image();
  std::vector<uint8_t> binary_data(string_data.begin(), string_data.end());

  image_decoder_->DecodeImage(
      binary_data,
      /*resize_to_notification_size=*/true,
      base::BindOnce(&FastPairRepositoryImpl::OnImageDecoded,
                     weak_ptr_factory_.GetWeakPtr(), normalized_model_id,
                     std::move(callback), *response));
}

void FastPairRepositoryImpl::OnImageDecoded(
    const std::string& normalized_model_id,
    DeviceMetadataCallback callback,
    nearby::fastpair::GetObservedDeviceResponse response,
    gfx::Image image) {
  metadata_cache_[normalized_model_id] =
      std::make_unique<DeviceMetadata>(response, std::move(image));
  std::move(callback).Run(metadata_cache_[normalized_model_id].get(),
                          /*has_retryable_error=*/false);
}

bool FastPairRepositoryImpl::IsAccountKeyPairedLocally(
    const std::vector<uint8_t>& account_key) {
  // Before we check if the |account_key| matches any of the devices saved in
  // the Saved Device Registry, we fetch all the devices saved to the user's
  // account and cross check the SHA256(concat(account_key, mac_address)) of
  // the saved devices with the SHA256(concat(account_key, mac_address)) of any
  // devices paired to the adapter using the paired device's mac address and
  // the given |account_key|. If there are any matches, we should add the
  // (mac_address, account_key) pair to the Saved Device Registry before
  // completing our check. This handles the edge case where a user pairs a
  // device already saved to their account from another platform via classic
  // BT pairing, and the device still emits a not discoverable advertisement,
  // and we want to prevent showing a Subsequent pairing notification in this
  // case.
  for (device::BluetoothDevice* device : adapter_->GetDevices()) {
    if (!device->IsPaired()) {
      continue;
    }

    // Use the paired device's |mac_address| and the given |account_key| to
    // generate a SHA256(concat(account_key, mac_address)), and use this
    // SHA256 hash to check if there are any matches with any saved devices in
    // Footprints.
    const std::string& mac_address = device->GetAddress();
    std::string paired_device_hash = GenerateSha256OfAccountKeyAndMacAddress(
        std::string(account_key.begin(), account_key.end()), mac_address);

    for (const auto& info : user_devices_cache_.fast_pair_info()) {
      if (info.has_device() &&
          info.device().has_sha256_account_key_public_address() &&
          info.device().sha256_account_key_public_address() ==
              paired_device_hash) {
        CD_LOG(VERBOSE, Feature::FP)
            << __func__
            << ": paired device already saved to account at address = "
            << mac_address << "; adding to registry";
        if (saved_device_registry_->SaveAccountAssociation(mac_address,
                                                           account_key)) {
          CD_LOG(VERBOSE, Feature::FP)
              << __func__ << ": paired device at address = " << mac_address
              << " added to local registry.";
        } else {
          CD_LOG(WARNING, Feature::FP)
              << __func__
              << ": failed to add paired device at address = " << mac_address
              << " to local registry.";
        }

        // We only expect there to be at most one match with |account_key| in
        // the devices saved to Footprints. An account key is uniquely written
        // to one device in the pairing flows. Since we found a match and
        // saved it locally, we can return "true" since the account key matches
        // a paired device.
        return true;
      }
    }
  }

  return saved_device_registry_->IsAccountKeySavedToRegistry(account_key);
}

void FastPairRepositoryImpl::CheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback) {
  CheckAccountKeysImpl(account_key_filter, std::move(callback),
                       /*allow_cache_refresh=*/true);
}

void FastPairRepositoryImpl::CheckAccountKeysImpl(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback,
    bool allow_cache_refresh) {
  CD_LOG(INFO, Feature::FP) << __func__;
  if (allow_cache_refresh &&
      (base::Time::Now() - footprints_last_updated_) > kCacheInvalidationTime) {
    // If it has been >30 minutes since the cache was updated, try to get
    // user devices from the server before proceeding.
    footprints_fetcher_->GetUserDevices(base::BindOnce(
        &FastPairRepositoryImpl::UpdateCacheAndRetryCheckAccountKeys,
        weak_ptr_factory_.GetWeakPtr(), account_key_filter,
        std::move(callback)));
    return;
  }

  for (const auto& info : user_devices_cache_.fast_pair_info()) {
    // We have to check that the devices in Footprints don't use the "forget
    // pattern" which Android uses in some cases to mark a device as removed
    // from the user's account.
    if (!info.has_device() || DoesDeviceHaveForgetPattern(info.device())) {
      continue;
    }

    const std::string& device_account_key_str = info.device().account_key();
    const std::vector<uint8_t> key_bytes(device_account_key_str.begin(),
                                         device_account_key_str.end());
    if (account_key_filter.IsAccountKeyInFilter(key_bytes)) {
      nearby::fastpair::StoredDiscoveryItem device;
      if (device.ParseFromString(info.device().discovery_item_bytes())) {
        CD_LOG(INFO, Feature::FP)
            << "Account key matched with a paired device: " << device.title();
        GetDeviceMetadata(
            device.id(),
            base::BindOnce(&FastPairRepositoryImpl::CompleteAccountKeyLookup,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(key_bytes)));
        return;
      }
    }
  }

  // On cache miss, query the server to make sure the device isn't saved to the
  // account unless we've already queried the server in the past minute.
  if (allow_cache_refresh &&
      (base::Time::Now() - footprints_last_updated_) > base::Minutes(1)) {
    footprints_fetcher_->GetUserDevices(
        base::BindOnce(&FastPairRepositoryImpl::RetryCheckAccountKeys,
                       weak_ptr_factory_.GetWeakPtr(), account_key_filter,
                       std::move(callback)));
    return;
  }

  std::move(callback).Run(std::nullopt);
}

void FastPairRepositoryImpl::RetryCheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback,
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  CD_LOG(INFO, Feature::FP) << __func__;
  if (!user_devices) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  UpdateUserDevicesCache(user_devices);
  CheckAccountKeysImpl(account_key_filter, std::move(callback),
                       /*allow_cache_refresh=*/false);
}

void FastPairRepositoryImpl::UpdateCacheAndRetryCheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback,
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  CD_LOG(INFO, Feature::FP) << __func__;
  if (!user_devices) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << "Failed to update user devices cache. Using stale cache";
  } else {
    UpdateUserDevicesCache(user_devices);
  }
  CheckAccountKeysImpl(account_key_filter, std::move(callback),
                       /*allow_cache_refresh=*/false);
}

void FastPairRepositoryImpl::CompleteAccountKeyLookup(
    CheckAccountKeysCallback callback,
    std::vector<uint8_t> account_key,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(
      PairingMetadata(device_metadata, std::move(account_key)));
}

void FastPairRepositoryImpl::UpdateUserDevicesCache(
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  if (user_devices) {
    CD_LOG(VERBOSE, Feature::FP)
        << "Updated user devices cache with "
        << user_devices->fast_pair_info_size() << " devices.";
    user_devices_cache_ = std::move(*user_devices);
    footprints_last_updated_ = base::Time::Now();
  }
}

void FastPairRepositoryImpl::WriteAccountAssociationToFootprints(
    scoped_refptr<Device> device,
    const std::vector<uint8_t>& account_key) {
  CD_LOG(INFO, Feature::FP) << __func__;
  DCHECK(device->classic_address());
  GetDeviceMetadata(
      device->metadata_id(),
      base::BindOnce(&FastPairRepositoryImpl::
                         WriteAccountAssociationToFootprintsWithMetadata,
                     weak_ptr_factory_.GetWeakPtr(), device->metadata_id(),
                     device->classic_address().value(), device->display_name(),
                     account_key, device->protocol()));
}

bool FastPairRepositoryImpl::WriteAccountAssociationToLocalRegistry(
    scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  std::optional<std::vector<uint8_t>> account_key = device->account_key();
  if (!account_key) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Account key not found for device.";
    return false;
  }

  DCHECK(device->classic_address());
  const std::string& mac_address = device->classic_address().value();
  if (saved_device_registry_->SaveAccountAssociation(mac_address,
                                                     account_key.value())) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": paired device at address = " << mac_address
        << " added to local registry.";
    return true;
  }

  CD_LOG(WARNING, Feature::FP)
      << __func__
      << ": failed to add paired device at address = " << mac_address
      << " to local registry.";
  return false;
}

void FastPairRepositoryImpl::WriteAccountAssociationToFootprintsWithMetadata(
    const std::string& hex_model_id,
    const std::string& mac_address,
    const std::optional<std::string>& display_name,
    const std::vector<uint8_t>& account_key,
    std::optional<Protocol> device_protocol,
    DeviceMetadata* metadata,
    bool has_retryable_error) {
  if (!metadata) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Unable to retrieve metadata.";
    return;
  }

  const nearby::fastpair::FastPairInfo fast_pair_info = BuildFastPairInfo(
      hex_model_id, account_key, mac_address, display_name, metadata);

  pending_write_store_->WritePairedDevice(mac_address, fast_pair_info);
  footprints_fetcher_->AddUserFastPairInfo(
      fast_pair_info,
      base::BindOnce(&FastPairRepositoryImpl::
                         OnWriteAccountAssociationToFootprintsComplete,
                     weak_ptr_factory_.GetWeakPtr(), mac_address, account_key,
                     device_protocol));
}

void FastPairRepositoryImpl::OnWriteAccountAssociationToFootprintsComplete(
    const std::string& mac_address,
    const std::vector<uint8_t>& account_key,
    std::optional<Protocol> device_protocol,
    bool success) {
  if (!success) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to write device to Footprints--"
           "deferring addition to SavedDeviceRegistry until we succeed.";
    return;
  }
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Successfully added device to Footprints.";

  // TODO(b/261917790): Capture a pending successful Footprint write in the
  // Retroactive Pairing Flow.
  if (device_protocol.has_value() &&
      device_protocol.value() == Protocol::kFastPairRetroactive) {
    RecordRetroactiveSuccessFunnelFlow(
        FastPairRetroactiveSuccessFunnelEvent::kSaveComplete);
  }

  // Remove pending write on successful Footprints write.
  pending_write_store_->OnPairedDeviceSaved(mac_address);
}

void FastPairRepositoryImpl::CheckOptInStatus(
    CheckOptInStatusCallback callback) {
  footprints_fetcher_->GetUserDevices(
      base::BindOnce(&FastPairRepositoryImpl::OnCheckOptInStatus,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FastPairRepositoryImpl::OnCheckOptInStatus(
    CheckOptInStatusCallback callback,
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  CD_LOG(INFO, Feature::FP) << __func__;
  if (!user_devices) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Missing UserReadDevicesResponse from call to Footprints";
    std::move(callback).Run(nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
    return;
  }

  for (const auto& info : user_devices->fast_pair_info()) {
    if (info.has_opt_in_status()) {
      std::move(callback).Run(info.opt_in_status());
      return;
    }
  }

  std::move(callback).Run(nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
}

void FastPairRepositoryImpl::UpdateOptInStatus(
    nearby::fastpair::OptInStatus opt_in_status,
    UpdateOptInStatusCallback callback) {
  footprints_fetcher_->AddUserFastPairInfo(
      BuildFastPairInfoForOptIn(opt_in_status),
      base::BindOnce(&FastPairRepositoryImpl::OnUpdateOptInStatusComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FastPairRepositoryImpl::OnUpdateOptInStatusComplete(
    UpdateOptInStatusCallback callback,
    bool success) {
  CD_LOG(INFO, Feature::FP) << __func__ << ": success=" << success;
  std::move(callback).Run(success);
}

void FastPairRepositoryImpl::GetSavedDevices(GetSavedDevicesCallback callback) {
  footprints_fetcher_->GetUserDevices(
      base::BindOnce(&FastPairRepositoryImpl::OnGetSavedDevices,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FastPairRepositoryImpl::OnGetSavedDevices(
    GetSavedDevicesCallback callback,
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  RecordGetSavedDevicesResult(/*success=*/user_devices.has_value());

  // |user_devices| will be null if we either didn't get a response from the
  // Footprints server or if we could not parse the response. Therefore we
  // should bubble up an error status and empty device list to the UI.
  if (!user_devices.has_value()) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Missing UserReadDevicesResponse from call to Footprints";
    std::move(callback).Run(nearby::fastpair::OptInStatus::
                                STATUS_ERROR_RETRIEVING_FROM_FOOTPRINTS_SERVER,
                            /*saved_device_list=*/{});
    return;
  }

  nearby::fastpair::OptInStatus opt_in_status =
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN;
  std::vector<nearby::fastpair::FastPairDevice> saved_devices;
  for (const auto& info : user_devices->fast_pair_info()) {
    if (info.has_opt_in_status()) {
      opt_in_status = info.opt_in_status();
    }

    // We have to check that the devices in Footprints don't use the "forget
    // pattern" which Android uses in some cases to mark a device as removed
    // from the user's account.
    if (!info.has_device() || DoesDeviceHaveForgetPattern(info.device())) {
      continue;
    }

    saved_devices.push_back(info.device());
  }

  // If the opt in status is `STATUS_OPTED_OUT`, then we can expect the list of
  // saved devices to be empty, since an opted out status removes all saved
  // devices from the list, although there still might be saved devices, if
  // an Android or Chromebook writes to the user's account against their wishes.
  std::move(callback).Run(opt_in_status, std::move(saved_devices));
}

void FastPairRepositoryImpl::DeleteAssociatedDevice(
    const std::string& mac_address,
    DeleteAssociatedDeviceCallback callback) {
  std::optional<const std::vector<uint8_t>> account_key =
      saved_device_registry_->GetAccountKey(mac_address);
  if (!account_key) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": No saved account key.";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  std::string hex_account_key = base::HexEncode(*account_key);

  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": Removing device from Footprints with address: " << mac_address;
  pending_write_store_->DeletePairedDevice(mac_address, hex_account_key);
  footprints_fetcher_->DeleteUserDevice(
      hex_account_key,
      base::BindOnce(&FastPairRepositoryImpl::OnDeleteAssociatedDevice,
                     weak_ptr_factory_.GetWeakPtr(), mac_address,
                     std::move(callback)));
}

void FastPairRepositoryImpl::UpdateAssociatedDeviceFootprintsName(
    const std::string& mac_address,
    const std::string& display_name,
    bool cache_may_be_stale) {
  std::optional<const std::vector<uint8_t>> account_key =
      saved_device_registry_->GetAccountKey(mac_address);
  if (!account_key.has_value()) {
    // If the device does not have an account key it must not be saved. If the
    // device was not saved, there is nothing to update. Log a warning and
    // return.
    CD_LOG(WARNING, Feature::FP) << __func__ << ": No saved account key.";
    return;
  }
  std::string account_key_str =
      std::string(account_key->begin(), account_key->end());

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": changing device display name to: " << display_name;

  // First check if the device is already in |user_devices_cache_| before
  // querying the server for it.
  for (const auto& info : user_devices_cache_.fast_pair_info()) {
    if (!info.has_device()) {
      continue;
    }
    const std::string& device_account_key_str = info.device().account_key();

    if (account_key_str == device_account_key_str) {
      nearby::fastpair::StoredDiscoveryItem item;
      item.ParseFromString(info.device().discovery_item_bytes());

      // Write to Footprints with the new device name.
      GetDeviceMetadata(
          item.id(),
          base::BindOnce(&FastPairRepositoryImpl::
                             WriteAccountAssociationToFootprintsWithMetadata,
                         weak_ptr_factory_.GetWeakPtr(), item.id(), mac_address,
                         display_name, account_key.value(),
                         /*device_protocol=*/std::nullopt));

      // Update |footprints_last_updated_| to make the cache invalid after the
      // name change.
      footprints_last_updated_ = base::Time();
      return;
    }
  }

  // If it's our first time through, then |cache_may_be_stale| will be set to
  // true in which case we refresh the cache and try again.
  if (cache_may_be_stale) {
    footprints_fetcher_->GetUserDevices(base::BindOnce(
        &FastPairRepositoryImpl::UpdateCacheAndRetryChangeDisplayName,
        weak_ptr_factory_.GetWeakPtr(), mac_address, display_name));
  }
}

void FastPairRepositoryImpl::UpdateCacheAndRetryChangeDisplayName(
    const std::string& mac_address,
    const std::string& display_name,
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  CD_LOG(INFO, Feature::FP) << __func__;
  if (user_devices) {
    UpdateUserDevicesCache(user_devices);
  } else {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to update user devices cache.";
    return;
  }

  // Perform the retry. The cache has now been refreshed so set
  // |cache_may_be_stale| to false to prevent calling this function again,
  // infinitely.
  FastPairRepositoryImpl::UpdateAssociatedDeviceFootprintsName(
      mac_address, display_name, /*cache_may_be_stale=*/false);
}

void FastPairRepositoryImpl::OnDeleteAssociatedDevice(
    const std::string& mac_address,
    DeleteAssociatedDeviceCallback callback,
    bool success) {
  if (!success) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to remove device from Footprints--"
           "deferring removal from SavedDeviceRegistry until we succeed.";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Successfully removed device from Footprints.";

  // Remove pending delete on successful Footprints delete.
  pending_write_store_->OnPairedDeviceDeleted(mac_address);

  // Query the server and update the cache so the removal is reflected.
  footprints_fetcher_->GetUserDevices(
      base::BindOnce(&FastPairRepositoryImpl::UpdateUserDevicesCache,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!saved_device_registry_->GetAccountKey(mac_address).has_value()) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": Device was already removed from Saved Device Registry.";
    std::move(callback).Run(/*success=*/true);
    return;
  }

  if (saved_device_registry_->DeleteAccountKey(mac_address)) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": Successfully removed device from Saved Device Registry.";
    std::move(callback).Run(/*success=*/true);
    return;
  }

  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": Failed to remove device from Saved Device Registry.";
  std::move(callback).Run(/*success=*/false);
}

void FastPairRepositoryImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  // Only retry when we have an active connected network.
  if (!network || !network->IsConnectedState()) {
    return;
  }

  // To prevent API call spam, only try to retry once per timeout.
  if ((base::Time::Now() - retry_write_or_delete_last_attempted_) <
      kOfflineRetryTimeout) {
    return;
  }

  retry_write_or_delete_last_attempted_ = base::Time::Now();

  // A call to |GetSavedDevices| isn't necessary; we don't have to check
  // the devices' most recent footprint before retrying a write since, in the
  // worst case, an already saved device will be updated with the same
  // information.
  if (!pending_write_store_->GetPendingWrites().empty()) {
    RetryPendingWrites();
  }

  // We must check whether there is a footprint in Footprints for the device
  // we want to delete; otherwise, the delete will fail, and the delete
  // will remain in the PendingDelete store forever.
  if (!pending_write_store_->GetPendingDeletes().empty()) {
    GetSavedDevices(base::BindOnce(&FastPairRepositoryImpl::RetryPendingDeletes,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void FastPairRepositoryImpl::RetryPendingWrites() {
  for (const PendingWriteStore::PendingWrite& pending_write :
       pending_write_store_->GetPendingWrites()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Retrying write for device with mac address: "
        << pending_write.mac_address;

    // Parse device account key from device fast pair info.
    const std::string& account_key_str =
        pending_write.fast_pair_info.device().account_key();
    std::vector<uint8_t> account_key =
        std::vector<uint8_t>{account_key_str.begin(), account_key_str.end()};

    footprints_fetcher_->AddUserFastPairInfo(
        pending_write.fast_pair_info,
        base::BindOnce(&FastPairRepositoryImpl::
                           OnWriteAccountAssociationToFootprintsComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       pending_write.mac_address, account_key,
                       /*device_protocol=*/std::nullopt));
  }
}

// Parameter |status| is passed but not used.
void FastPairRepositoryImpl::RetryPendingDeletes(
    nearby::fastpair::OptInStatus status,
    std::vector<nearby::fastpair::FastPairDevice> devices) {
  // For each pending delete, check to see if the account key is stored in
  // Footprints. While the device failed to delete on this Chromebook, it could
  // have been successfully deleted on a different device.
  for (const PendingWriteStore::PendingDelete& pending_delete :
       pending_write_store_->GetPendingDeletes()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Checking if failed delete should be retried "
           "for account key: "
        << pending_delete.hex_account_key;

    // Check if this pending delete is for a device that is in Footprints.
    bool found_in_saved_devices = false;
    for (const auto& device : devices) {
      DCHECK(device.has_account_key());

      const std::string saved_account_key =
          base::HexEncode(device.account_key());
      found_in_saved_devices =
          saved_account_key == pending_delete.hex_account_key;
      if (found_in_saved_devices) {
        break;
      }
    }

    // If our failed-to-delete account key is still found in Footprints, then
    // proceed with retrying the delete.
    if (found_in_saved_devices) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": Retrying delete for device with account key "
          << pending_delete.hex_account_key;
      footprints_fetcher_->DeleteUserDevice(
          pending_delete.hex_account_key,
          base::BindOnce(&FastPairRepositoryImpl::OnDeleteAssociatedDevice,
                         weak_ptr_factory_.GetWeakPtr(),
                         pending_delete.mac_address, base::DoNothing()));
    } else if (saved_device_registry_->GetAccountKey(pending_delete.mac_address)
                   .has_value()) {
      // If the device was already removed from Footprints, but hasn't been
      // removed from the registry, ensure that we remove it from the registry.
      bool result =
          saved_device_registry_->DeleteAccountKey(pending_delete.mac_address);
      CD_LOG(INFO, Feature::FP)
          << __func__
          << ": Device removed from Footprints, removing from "
             "SavedDeviceRegistry was "
          << (result ? "sucessful." : "unsuccessful.");

      // Remove from our list of pending deletes since the device isn't in
      // Footprints.
      pending_write_store_->OnPairedDeviceDeleted(pending_delete.mac_address);
    }
  }
}

void FastPairRepositoryImpl::DeleteAssociatedDeviceByAccountKey(
    const std::vector<uint8_t>& account_key,
    DeleteAssociatedDeviceByAccountKeyCallback callback) {
  CD_LOG(INFO, Feature::FP) << __func__ << ": Removing device from Footprints.";
  footprints_fetcher_->DeleteUserDevice(
      base::HexEncode(account_key),
      base::BindOnce(
          &FastPairRepositoryImpl::OnDeleteAssociatedDeviceByAccountKey,
          weak_ptr_factory_.GetWeakPtr(), account_key, std::move(callback)));
}

void FastPairRepositoryImpl::OnDeleteAssociatedDeviceByAccountKey(
    const std::vector<uint8_t>& account_key,
    DeleteAssociatedDeviceByAccountKeyCallback callback,
    bool footprints_removal_success) {
  // Remove pending delete on successful Footprints delete.
  pending_write_store_->OnPairedDeviceDeleted(account_key);

  bool saved_device_registry_removal_success =
      saved_device_registry_->DeleteAccountKey(account_key);

  CD_LOG(INFO, Feature::FP)
      << __func__
      << ": Device removal: from Footprints: " << footprints_removal_success
      << "; from SavedDeviceRegistry: "
      << saved_device_registry_removal_success;
  if (footprints_removal_success) {
    // If removing from footprints was successful, Query the server and update
    // the cache so the removal is reflected.
    footprints_fetcher_->GetUserDevices(
        base::BindOnce(&FastPairRepositoryImpl::UpdateUserDevicesCache,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  std::move(callback).Run(/*success=*/footprints_removal_success &&
                          saved_device_registry_removal_success);
}

void FastPairRepositoryImpl::FetchDeviceImages(scoped_refptr<Device> device) {
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Fetching device images for model ID "
      << device->metadata_id();
  // Save a record of the mac address -> model ID for this device so that we can
  // display images for device objects that lack a model ID, such as
  // device::BluetoothDevice.
  // TODO(b/235117226): The mac address to model ID mapping will always fail
  // when this function is called the first time during device discovery; this
  // is because the classic address is not known yet. We should split the mac
  // address to model ID mapping into it's own function (or PersistDeviceImages)
  // to reflect this.
  if (!device_address_map_->SaveModelIdForDevice(device)) {
    CD_LOG(WARNING, Feature::FP) << __func__
                                 << ": Unable to save mac address -> model ID"
                                    " mapping for model ID "
                                 << device->metadata_id();
  }

  GetDeviceMetadata(
      device->metadata_id(),
      base::BindOnce(&FastPairRepositoryImpl::CompleteFetchDeviceImages,
                     weak_ptr_factory_.GetWeakPtr(), device->metadata_id()));
}

std::optional<std::string>
FastPairRepositoryImpl::GetDeviceDisplayNameFromCache(
    std::vector<uint8_t> account_key) {
  std::string account_key_str =
      std::string(account_key.begin(), account_key.end());

  CD_LOG(INFO, Feature::FP) << __func__ << ": Scanning cache for device name.";
  for (const auto& info : user_devices_cache_.fast_pair_info()) {
    // We have to check that the devices in Footprints don't use the "forget
    // pattern" which Android uses in some cases to mark a device as removed
    // from the user's account.
    if (!info.has_device() || DoesDeviceHaveForgetPattern(info.device())) {
      continue;
    }

    const std::string& device_account_key_str = info.device().account_key();

    if (account_key_str == device_account_key_str) {
      nearby::fastpair::StoredDiscoveryItem item;
      item.ParseFromString(info.device().discovery_item_bytes());
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": Found display name: " << item.title();
      return item.title();
    }
  }
  return std::nullopt;
}

void FastPairRepositoryImpl::CompleteFetchDeviceImages(
    const std::string& hex_model_id,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No metadata available for " << hex_model_id;
    return;
  }

  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Completing fetching device images for model ID "
      << hex_model_id;
  device_image_store_->FetchDeviceImages(hex_model_id, device_metadata,
                                         base::DoNothing());
}

bool FastPairRepositoryImpl::PersistDeviceImages(scoped_refptr<Device> device) {
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Persisting device images for model ID "
      << device->metadata_id();
  if (!device_address_map_->PersistRecordsForDevice(device)) {
    CD_LOG(WARNING, Feature::FP) << __func__
                                 << ": Unable to persist address -> model ID"
                                    " mapping for model ID "
                                 << device->metadata_id();
    return false;
  }
  return device_image_store_->PersistDeviceImages(device->metadata_id());
}

bool FastPairRepositoryImpl::EvictDeviceImages(const std::string& mac_address) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": Evicting mac address to model ID record for: " << mac_address;

  // TODO(235117226): Remove the records associated with the BLE address.
  std::optional<const std::string> hex_model_id =
      device_address_map_->GetModelIdForMacAddress(mac_address);
  if (!hex_model_id) {
    return false;
  }
  device_address_map_->EvictMacAddressRecord(mac_address);

  // Before evicting images, check if other device IDs map to this model ID.
  if (device_address_map_->HasPersistedRecordsForModelId(
          hex_model_id.value())) {
    return false;
  }

  return device_image_store_->EvictDeviceImages(hex_model_id.value());
}

std::optional<bluetooth_config::DeviceImageInfo>
FastPairRepositoryImpl::GetImagesForDevice(const std::string& mac_address) {
  std::optional<const std::string> hex_model_id =
      device_address_map_->GetModelIdForMacAddress(mac_address);
  if (!hex_model_id) {
    return std::nullopt;
  }

  return device_image_store_->GetImagesForDeviceModel(hex_model_id.value());
}

void FastPairRepositoryImpl::IsDeviceSavedToAccount(
    const std::string& mac_address,
    IsDeviceSavedToAccountCallback callback) {
  footprints_fetcher_->GetUserDevices(base::BindOnce(
      &FastPairRepositoryImpl::CompleteIsDeviceSavedToAccount,
      weak_ptr_factory_.GetWeakPtr(), mac_address, std::move(callback)));
}

void FastPairRepositoryImpl::CompleteIsDeviceSavedToAccount(
    const std::string& mac_address,
    IsDeviceSavedToAccountCallback callback,
    std::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  CD_LOG(INFO, Feature::FP) << __func__;

  if (!user_devices) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Missing UserReadDevicesResponse from call to Footprints";
    std::move(callback).Run(false);
    return;
  }

  for (const auto& info : user_devices->fast_pair_info()) {
    if (info.has_device() &&
        IsDeviceSha256Matched(info.device(), mac_address)) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__
          << ": found a SHA256 match for device at address = " << mac_address;
      std::move(callback).Run(true);
      return;
    }
  }

  std::move(callback).Run(false);
}

}  // namespace quick_pair
}  // namespace ash
