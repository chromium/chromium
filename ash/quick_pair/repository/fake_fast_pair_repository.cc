// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fake_fast_pair_repository.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FakeFastPairRepository::FakeFastPairRepository() {
  SetInstanceForTesting(this);
}

FakeFastPairRepository::~FakeFastPairRepository() {
  SetInstanceForTesting(nullptr);
}

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
    std::optional<PairingMetadata> result) {
  check_account_keys_result_ = result;
}

bool FakeFastPairRepository::HasKeyForDevice(const std::string& mac_address) {
  return saved_account_keys_.contains(mac_address);
}

bool FakeFastPairRepository::HasNameForDevice(const std::string& mac_address) {
  return saved_display_names_.contains(mac_address);
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

void FakeFastPairRepository::WriteAccountAssociationToFootprints(
    scoped_refptr<Device> device,
    const std::vector<uint8_t>& account_key) {
  saved_account_keys_.insert_or_assign(device->classic_address().value(),
                                       account_key);
  saved_display_names_.insert_or_assign(device->classic_address().value(),
                                        device->display_name().value());
}

bool FakeFastPairRepository::WriteAccountAssociationToLocalRegistry(
    scoped_refptr<Device> device) {
  std::vector<uint8_t> fake_account_key;
  saved_account_keys_[device->classic_address().value()] = fake_account_key;
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

nearby::fastpair::OptInStatus FakeFastPairRepository::GetOptInStatus() {
  return status_;
}

// Unimplemented.
void FakeFastPairRepository::CheckOptInStatus(
    CheckOptInStatusCallback callback) {
  std::move(callback).Run(status_);
}

void FakeFastPairRepository::DeleteAssociatedDeviceByAccountKey(
    const std::vector<uint8_t>& account_key,
    DeleteAssociatedDeviceByAccountKeyCallback callback) {
  for (auto it = devices_.begin(); it != devices_.end(); it++) {
    if (it->has_account_key() &&
        base::HexEncode(it->account_key()) == base::HexEncode(account_key)) {
      devices_.erase(it);
      std::move(callback).Run(/*success=*/true);
      return;
    }
  }
  std::move(callback).Run(/*success=*/false);
}

void FakeFastPairRepository::UpdateAssociatedDeviceFootprintsName(
    const std::string& mac_address,
    const std::string& display_name,
    bool cache_may_be_stale) {
  saved_display_names_.insert_or_assign(mac_address, display_name);
}

void FakeFastPairRepository::UpdateOptInStatus(
    nearby::fastpair::OptInStatus opt_in_status,
    UpdateOptInStatusCallback callback) {
  status_ = opt_in_status;
  std::move(callback).Run(/*success=*/true);
}

// Unimplemented.
void FakeFastPairRepository::FetchDeviceImages(scoped_refptr<Device> device) {
  return;
}

// Unimplemented.
std::optional<std::string>
FakeFastPairRepository::GetDeviceDisplayNameFromCache(
    std::vector<uint8_t> account_key) {
  return nullptr;
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
bool FakeFastPairRepository::EvictDeviceImages(const std::string& mac_address) {
  return true;
}

// Unimplemented.
std::optional<bluetooth_config::DeviceImageInfo>
FakeFastPairRepository::GetImagesForDevice(const std::string& mac_address) {
  return std::nullopt;
}

void FakeFastPairRepository::SetSavedDevices(
    nearby::fastpair::OptInStatus status,
    std::vector<nearby::fastpair::FastPairDevice> devices) {
  status_ = status;
  devices_ = std::move(devices);
}

void FakeFastPairRepository::GetSavedDevices(GetSavedDevicesCallback callback) {
  std::move(callback).Run(status_, devices_);
}

void FakeFastPairRepository::SaveMacAddressToAccount(
    const std::string& mac_address) {
  saved_mac_addresses_.insert(mac_address);
}

void FakeFastPairRepository::IsDeviceSavedToAccount(
    const std::string& mac_address,
    IsDeviceSavedToAccountCallback callback) {
  if (saved_to_account_callback_is_delayed_) {
    saved_to_account_callback_ = std::move(callback);
    return;
  }

  if (base::Contains(saved_mac_addresses_, mac_address)) {
    std::move(callback).Run(true);
    return;
  }

  std::move(callback).Run(false);
}

void FakeFastPairRepository::TriggerIsDeviceSavedToAccountCallback() {
  std::move(saved_to_account_callback_).Run(false);
}

}  // namespace quick_pair
}  // namespace ash
