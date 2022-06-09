// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fake_fast_pair_repository.h"

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/strings/string_util.h"
#include "chromeos/services/bluetooth_config/public/cpp/device_image_info.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FakeFastPairRepository::FakeFastPairRepository() : FastPairRepository() {}

FakeFastPairRepository::~FakeFastPairRepository() = default;

void FakeFastPairRepository::SetFakeMetadata(const std::string& hex_model_id,
                                             nearby::fastpair::Device metadata,
                                             gfx::Image image) {
  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->CopyFrom(metadata);

  data_[base::ToUpperASCII(hex_model_id)] =
      std::make_unique<DeviceMetadata>(response, image);
}

void FakeFastPairRepository::ClearFakeMetadata(
    const std::string& hex_model_id) {
  data_.erase(base::ToUpperASCII(hex_model_id));
}

void FakeFastPairRepository::SetCheckAccountKeysResult(
    absl::optional<PairingMetadata> result) {
  check_account_keys_result_ = result;
}

bool FakeFastPairRepository::HasKeyForDevice(const std::string& mac_address) {
  return saved_account_keys_.contains(mac_address);
}

void FakeFastPairRepository::GetDeviceMetadata(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback) {
  if (!is_network_connected_) {
    std::move(callback).Run(/*device=*/nullptr, /*has_retryable_error=*/true);
    return;
  }

  std::string normalized_id = base::ToUpperASCII(hex_model_id);
  if (data_.contains(normalized_id)) {
    std::move(callback).Run(data_[normalized_id].get(),
                            /*has_retryable_error=*/false);
    return;
  }

  std::move(callback).Run(nullptr, /*has_retryable_error=*/true);
}

void FakeFastPairRepository::CheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback) {
  std::move(callback).Run(check_account_keys_result_);
}

void FakeFastPairRepository::AssociateAccountKey(
    scoped_refptr<Device> device,
    const std::vector<uint8_t>& account_key) {
  saved_account_keys_[device->ble_address] = account_key;
}

bool FakeFastPairRepository::AssociateAccountKeyLocally(
    scoped_refptr<Device> device) {
  std::vector<uint8_t> fake_account_key;
  saved_account_keys_[device->ble_address] = fake_account_key;
  return true;
}

void FakeFastPairRepository::DeleteAssociatedDevice(
    const std::string& mac_address,
    DeleteAssociatedDeviceCallback callback) {
  std::move(callback).Run(saved_account_keys_.erase(mac_address) == 1);
}

void FakeFastPairRepository::SetOptInStatus(
    nearby::fastpair::OptInStatus status) {
  status_ = status;
}

// Unimplemented.
void FakeFastPairRepository::CheckOptInStatus(
    CheckOptInStatusCallback callback) {
  std::move(callback).Run(status_);
}

// Unimplemented.
void FakeFastPairRepository::DeleteAssociatedDeviceByAccountKey(
    const std::vector<uint8_t>& account_key,
    DeleteAssociatedDeviceByAccountKeyCallback callback) {}

// Unimplemented.
void FakeFastPairRepository::UpdateOptInStatus(
    nearby::fastpair::OptInStatus opt_in_status,
    UpdateOptInStatusCallback callback) {}

// Unimplemented.
void FakeFastPairRepository::FetchDeviceImages(scoped_refptr<Device> device) {
  return;
}

bool FakeFastPairRepository::IsAccountKeyPairedLocally(
    const std::vector<uint8_t>& account_key) {
  return is_account_key_paired_locally_;
}

// Unimplemented.
bool FakeFastPairRepository::PersistDeviceImages(scoped_refptr<Device> device) {
  return true;
}

// Unimplemented.
bool FakeFastPairRepository::EvictDeviceImages(
    const device::BluetoothDevice* device) {
  return true;
}

// Unimplemented.
absl::optional<chromeos::bluetooth_config::DeviceImageInfo>
FakeFastPairRepository::GetImagesForDevice(const std::string& device_id) {
  return absl::nullopt;
}

// Unimplemented.
void FakeFastPairRepository::GetSavedDevices(GetSavedDevicesCallback callback) {

}

}  // namespace quick_pair
}  // namespace ash
