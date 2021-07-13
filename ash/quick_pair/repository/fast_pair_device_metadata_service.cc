// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_device_metadata_service.h"

#include "ash/quick_pair/common/logging.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FastPairDeviceMetadataService::FastPairDeviceMetadataService() = default;
FastPairDeviceMetadataService::~FastPairDeviceMetadataService() = default;

void FastPairDeviceMetadataService::GetDeviceMetadata(
    const std::string& hex_model_id,
    base::OnceCallback<void(absl::optional<nearby::fastpair::Device>)>
        callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(absl::nullopt);
}

void FastPairDeviceMetadataService::IsValidModelId(
    const std::string& hex_model_id,
    base::OnceCallback<void(bool)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(false);
}

void FastPairDeviceMetadataService::GetAssociatedAccountKey(
    const std::string& address,
    const std::string& account_key_filter,
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  QP_LOG(INFO) << __func__;
  std::move(callback).Run(absl::nullopt);
}

void FastPairDeviceMetadataService::AssociateAccountKey(
    const Device& device,
    const std::string& account_key) {
  QP_LOG(INFO) << __func__;
}

void FastPairDeviceMetadataService::DeleteAssociatedDevice(
    const device::BluetoothDevice* device) {
  QP_LOG(INFO) << __func__;
}

}  // namespace quick_pair
}  // namespace ash
