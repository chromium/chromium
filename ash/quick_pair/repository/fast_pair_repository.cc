// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
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
    : device_metadata_fetcher_(std::make_unique<DeviceMetadataFetcher>()) {
  SetInstance(this);
}

FastPairRepository::~FastPairRepository() {
  SetInstance(nullptr);
}

void FastPairRepository::GetDeviceMetadata(
    const std::string& hex_model_id,
    base::OnceCallback<
        void(absl::optional<nearby::fastpair::GetObservedDeviceResponse>)>
        callback) {
  QP_LOG(INFO) << __func__;
  device_metadata_fetcher_->LookupHexDeviceId(hex_model_id,
                                              std::move(callback));
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
