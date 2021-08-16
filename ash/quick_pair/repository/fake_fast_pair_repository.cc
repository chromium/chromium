// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fake_fast_pair_repository.h"

#include "base/strings/string_util.h"

namespace ash {
namespace quick_pair {

FakeFastPairRepository::FakeFastPairRepository() : FastPairRepository() {}

FakeFastPairRepository::~FakeFastPairRepository() = default;

void FakeFastPairRepository::SetFakeMetadata(const std::string& hex_model_id,
                                             nearby::fastpair::Device metadata,
                                             gfx::Image image) {
  data_[base::ToUpperASCII(hex_model_id)] =
      std::make_unique<DeviceMetadata>(metadata, image);
}

void FakeFastPairRepository::ClearFakeMetadata(
    const std::string& hex_model_id) {
  data_.erase(base::ToUpperASCII(hex_model_id));
}

void FakeFastPairRepository::GetDeviceMetadata(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback) {
  std::string normalized_id = base::ToUpperASCII(hex_model_id);
  if (data_.contains(normalized_id)) {
    std::move(callback).Run(data_[normalized_id].get());
    return;
  }

  std::move(callback).Run(nullptr);
}

void FakeFastPairRepository::IsValidModelId(
    const std::string& hex_model_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(data_.contains(base::ToUpperASCII(hex_model_id)));
}

void FakeFastPairRepository::GetAssociatedAccountKey(
    const std::string& address,
    const std::string& account_key_filter,
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  std::move(callback).Run(absl::nullopt);
}

void FakeFastPairRepository::AssociateAccountKey(
    const Device& device,
    const std::string& account_key) {}

void FakeFastPairRepository::DeleteAssociatedDevice(
    const device::BluetoothDevice* device) {}

}  // namespace quick_pair
}  // namespace ash
