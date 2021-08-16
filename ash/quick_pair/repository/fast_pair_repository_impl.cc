// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository_impl.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FastPairRepositoryImpl::FastPairRepositoryImpl()
    : FastPairRepository(),
      device_metadata_fetcher_(std::make_unique<DeviceMetadataFetcher>()),
      image_decoder_(std::make_unique<FastPairImageDecoder>(
          std::unique_ptr<image_fetcher::ImageFetcher>())) {}

FastPairRepositoryImpl::~FastPairRepositoryImpl() = default;

void FastPairRepositoryImpl::GetDeviceMetadata(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback) {
  std::string normalized_id = base::ToUpperASCII(hex_model_id);
  if (metadata_cache_.contains(normalized_id)) {
    QP_LOG(VERBOSE) << __func__ << "Data already in cache.";
    std::move(callback).Run(metadata_cache_[normalized_id].get());
    return;
  }
  QP_LOG(VERBOSE) << __func__ << "Not cached, fetching from web service.";
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
        std::make_unique<DeviceMetadata>(response->device(), gfx::Image());
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
      std::make_unique<DeviceMetadata>(response.device(), std::move(image));
  std::move(callback).Run(metadata_cache_[normalized_model_id].get());
}

void FastPairRepositoryImpl::IsValidModelId(
    const std::string& hex_model_id,
    base::OnceCallback<void(bool)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(false);
}

void FastPairRepositoryImpl::GetAssociatedAccountKey(
    const std::string& address,
    const std::string& account_key_filter,
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(absl::nullopt);
}

void FastPairRepositoryImpl::AssociateAccountKey(
    const Device& device,
    const std::string& account_key) {
  QP_LOG(INFO) << __func__;
}

void FastPairRepositoryImpl::DeleteAssociatedDevice(
    const device::BluetoothDevice* device) {
  QP_LOG(INFO) << __func__;
}

}  // namespace quick_pair
}  // namespace ash
