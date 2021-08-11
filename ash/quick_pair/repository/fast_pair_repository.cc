// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FastPairRepository* g_instance = nullptr;

// static
FastPairRepository* FastPairRepository::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void FastPairRepository::SetInstance(FastPairRepository* instance) {
  DCHECK(!g_instance);
  g_instance = instance;
}

FastPairRepository::FastPairRepository()
    : device_metadata_fetcher_(std::make_unique<DeviceMetadataFetcher>()),
      image_decoder_(std::make_unique<FastPairImageDecoder>(
          std::unique_ptr<image_fetcher::ImageFetcher>())) {
  SetInstance(this);
}

FastPairRepository::~FastPairRepository() {
  SetInstance(nullptr);
}

void FastPairRepository::GetDeviceMetadata(const std::string& hex_model_id,
                                           DeviceMetadataCallback callback) {
  if (metadata_cache_.contains(hex_model_id)) {
    QP_LOG(VERBOSE) << __func__ << "Data already in cache.";
    std::move(callback).Run(metadata_cache_[hex_model_id].get());
    return;
  }
  QP_LOG(VERBOSE) << __func__ << "Not cached, fetching from web service.";
  device_metadata_fetcher_->LookupHexDeviceId(
      hex_model_id, base::BindOnce(&FastPairRepository::OnMetadataFetched,
                                   weak_ptr_factory_.GetWeakPtr(), hex_model_id,
                                   std::move(callback)));
}

void FastPairRepository::OnMetadataFetched(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback,
    absl::optional<nearby::fastpair::GetObservedDeviceResponse> response) {
  if (!response) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (response->image().empty()) {
    metadata_cache_[hex_model_id] =
        std::make_unique<DeviceMetadata>(response->device(), gfx::Image());
    std::move(callback).Run(metadata_cache_[hex_model_id].get());
    return;
  }

  const std::string& string_data = response->image();
  std::vector<uint8_t> binary_data(string_data.begin(), string_data.end());

  image_decoder_->DecodeImage(
      binary_data, base::BindOnce(&FastPairRepository::OnImageDecoded,
                                  weak_ptr_factory_.GetWeakPtr(), hex_model_id,
                                  std::move(callback), *response));
}

void FastPairRepository::OnImageDecoded(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback,
    nearby::fastpair::GetObservedDeviceResponse response,
    gfx::Image image) {
  metadata_cache_[hex_model_id] =
      std::make_unique<DeviceMetadata>(response.device(), std::move(image));
  std::move(callback).Run(metadata_cache_[hex_model_id].get());
}

void FastPairRepository::IsValidModelId(
    const std::string& hex_model_id,
    base::OnceCallback<void(bool)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(false);
}

void FastPairRepository::GetAssociatedAccountKey(
    const std::string& address,
    const std::string& account_key_filter,
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(absl::nullopt);
}

void FastPairRepository::AssociateAccountKey(const Device& device,
                                             const std::string& account_key) {
  QP_LOG(INFO) << __func__;
}

void FastPairRepository::DeleteAssociatedDevice(
    const device::BluetoothDevice* device) {
  QP_LOG(INFO) << __func__;
}

}  // namespace quick_pair
}  // namespace ash
