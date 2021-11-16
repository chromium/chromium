// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository_impl.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair/footprints_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/proto_conversions.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FastPairRepositoryImpl::FastPairRepositoryImpl()
    : FastPairRepository(),
      device_metadata_fetcher_(std::make_unique<DeviceMetadataFetcher>()),
      footprints_fetcher_(std::make_unique<FootprintsFetcher>()),
      image_decoder_(std::make_unique<FastPairImageDecoder>(
          std::unique_ptr<image_fetcher::ImageFetcher>())),
      saved_device_registry_(std::make_unique<SavedDeviceRegistry>()),
      footprints_last_updated_(base::Time::UnixEpoch()) {
  // TODO(crbug/1270534): Determine the best place to make this call.
  // footprints_fetcher_->GetUserDevices(
  //     base::BindOnce(&FastPairRepositoryImpl::UpdateUserDevicesCache,
  //                    weak_ptr_factory_.GetWeakPtr()));
}

FastPairRepositoryImpl::~FastPairRepositoryImpl() = default;

void FastPairRepositoryImpl::GetDeviceMetadata(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback) {
  std::string normalized_id = base::ToUpperASCII(hex_model_id);
  if (metadata_cache_.contains(normalized_id)) {
    QP_LOG(VERBOSE) << __func__ << ": Data already in cache.";
    std::move(callback).Run(metadata_cache_[normalized_id].get());
    return;
  }
  QP_LOG(VERBOSE) << __func__ << ": Not cached, fetching from web service.";
  device_metadata_fetcher_->LookupHexDeviceId(
      normalized_id, base::BindOnce(&FastPairRepositoryImpl::OnMetadataFetched,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    normalized_id, std::move(callback)));
}

void FastPairRepositoryImpl::OnMetadataFetched(
    const std::string& normalized_model_id,
    DeviceMetadataCallback callback,
    absl::optional<nearby::fastpair::GetObservedDeviceResponse> response) {
  if (!response) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (response->image().empty()) {
    metadata_cache_[normalized_model_id] =
        std::make_unique<DeviceMetadata>(std::move(*response), gfx::Image());
    std::move(callback).Run(metadata_cache_[normalized_model_id].get());
    return;
  }

  const std::string& string_data = response->image();
  std::vector<uint8_t> binary_data(string_data.begin(), string_data.end());

  image_decoder_->DecodeImage(
      binary_data,
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
  std::move(callback).Run(metadata_cache_[normalized_model_id].get());
}

void FastPairRepositoryImpl::IsValidModelId(
    const std::string& hex_model_id,
    base::OnceCallback<void(bool)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(false);
}

void FastPairRepositoryImpl::CheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback) {
  CheckAccountKeysImpl(account_key_filter, std::move(callback),
                       /*refresh_cache_on_miss=*/true);
}

void FastPairRepositoryImpl::CheckAccountKeysImpl(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback,
    bool refresh_cache_on_miss) {
  QP_LOG(INFO) << __func__;
  for (const auto& info : user_devices_cache_.fast_pair_info()) {
    if (info.has_device()) {
      const std::string& string_key = info.device().account_key();
      const std::vector<uint8_t> binary_key(string_key.begin(),
                                            string_key.end());
      if (account_key_filter.Test(binary_key)) {
        nearby::fastpair::StoredDiscoveryItem device;
        if (device.ParseFromString(info.device().discovery_item_bytes())) {
          QP_LOG(INFO) << "Account key matched with a paired device: "
                       << device.title();
          GetDeviceMetadata(
              device.id(),
              base::BindOnce(&FastPairRepositoryImpl::CompleteAccountKeyLookup,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), std::move(binary_key)));
          return;
        }
      }
    }
  }

  if (refresh_cache_on_miss &&
      (base::Time::Now() - footprints_last_updated_) > base::Minutes(1)) {
    footprints_fetcher_->GetUserDevices(
        base::BindOnce(&FastPairRepositoryImpl::RetryCheckAccountKeys,
                       weak_ptr_factory_.GetWeakPtr(), account_key_filter,
                       std::move(callback)));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

void FastPairRepositoryImpl::RetryCheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback,
    absl::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  if (!user_devices) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  UpdateUserDevicesCache(user_devices);
  CheckAccountKeysImpl(account_key_filter, std::move(callback),
                       /*refresh_cache_on_miss=*/false);
}

void FastPairRepositoryImpl::CompleteAccountKeyLookup(
    CheckAccountKeysCallback callback,
    const std::vector<uint8_t> account_key,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(
      PairingMetadata(device_metadata, std::move(account_key)));
}

void FastPairRepositoryImpl::UpdateUserDevicesCache(
    absl::optional<nearby::fastpair::UserReadDevicesResponse> user_devices) {
  if (user_devices) {
    QP_LOG(VERBOSE) << "Updated user devices cache with "
                    << user_devices->fast_pair_info_size() << " devices.";
    user_devices_cache_ = std::move(*user_devices);
    footprints_last_updated_ = base::Time::Now();
  }
}

void FastPairRepositoryImpl::AssociateAccountKey(
    scoped_refptr<Device> device,
    const std::vector<uint8_t>& account_key) {
  QP_LOG(INFO) << __func__;
  DCHECK(device->classic_address());
  GetDeviceMetadata(
      device->metadata_id,
      base::BindOnce(&FastPairRepositoryImpl::AddToFootprints,
                     weak_ptr_factory_.GetWeakPtr(), device->metadata_id,
                     device->classic_address().value(), account_key));
}

void FastPairRepositoryImpl::AddToFootprints(
    const std::string& hex_model_id,
    const std::string& mac_address,
    const std::vector<uint8_t>& account_key,
    DeviceMetadata* metadata) {
  if (!metadata) {
    QP_LOG(WARNING) << __func__ << ": Unable to retrieve metadata.";
    return;
  }

  footprints_fetcher_->AddUserDevice(
      BuildFastPairInfo(hex_model_id, account_key, metadata),
      base::BindOnce(&FastPairRepositoryImpl::OnAddToFootprintsComplete,
                     weak_ptr_factory_.GetWeakPtr(), mac_address, account_key));
}

void FastPairRepositoryImpl::OnAddToFootprintsComplete(
    const std::string& mac_address,
    const std::vector<uint8_t>& account_key,
    bool success) {
  if (!success) {
    // TODO(jonmann): Handle caching to disk + retries.
    return;
  }

  saved_device_registry_->SaveAccountKey(mac_address, account_key);
}

bool FastPairRepositoryImpl::DeleteAssociatedDevice(
    const device::BluetoothDevice* device) {
  QP_LOG(INFO) << __func__;
  absl::optional<const std::vector<uint8_t>> account_key =
      saved_device_registry_->GetAccountKey(device->GetAddress());
  if (!account_key) {
    QP_LOG(VERBOSE)
        << __func__
        << ": Cannot find matching account key for unpaired device.";
    return false;
  }

  QP_LOG(INFO) << __func__ << ": Removing device from Footprints.";
  footprints_fetcher_->DeleteUserDevice(base::HexEncode(*account_key),
                                        base::DoNothing());
  // TODO(jonmann): Handle saving pending update to disk + retries.
  return true;
}

}  // namespace quick_pair
}  // namespace ash
