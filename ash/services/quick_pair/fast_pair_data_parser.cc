// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/fast_pair_data_parser.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/services/quick_pair/fast_pair_decryption.h"
#include "ash/services/quick_pair/public/cpp/battery_notification.h"
#include "ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "base/containers/flat_map.h"
#include "crypto/openssl_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr int kHeaderIndex = 0;
constexpr int kFieldTypeBitmask = 0b00001111;
constexpr int kFieldLengthBitmask = 0b11110000;
constexpr int kHeaderLength = 1;
constexpr int kFieldLengthOffset = 4;
constexpr int kFieldTypeAccountKeyFilter = 0;
constexpr int kFieldTypeAccountKeyFilterSalt = 1;
constexpr int kFieldTypeAccountKeyFilterNoNotification = 2;
constexpr int kFieldTypeBattery = 3;
constexpr int kFieldTypeBatteryNoNotification = 4;

bool ValidateInputSizes(const std::vector<uint8_t>& aes_key_bytes,
                        const std::vector<uint8_t>& encrypted_bytes) {
  if (aes_key_bytes.size() != kAesBlockByteSize) {
    QP_LOG(WARNING) << __func__
                    << ": AES key should have size = " << kAesBlockByteSize
                    << ", actual =  " << aes_key_bytes.size();
    return false;
  }

  if (encrypted_bytes.size() != kEncryptedDataByteSize) {
    QP_LOG(WARNING) << __func__ << ": Encrypted bytes should have size = "
                    << kEncryptedDataByteSize
                    << ", actual =  " << encrypted_bytes.size();
    return false;
  }

  return true;
}

void ConvertVectorsToArrays(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_bytes,
    std::array<uint8_t, kAesBlockByteSize>& out_aes_key_bytes,
    std::array<uint8_t, kEncryptedDataByteSize>& out_encrypted_bytes) {
  std::copy(aes_key_bytes.begin(), aes_key_bytes.end(),
            out_aes_key_bytes.begin());
  std::copy(encrypted_bytes.begin(), encrypted_bytes.end(),
            out_encrypted_bytes.begin());
}

}  // namespace

namespace ash {
namespace quick_pair {

FastPairDataParser::FastPairDataParser(
    mojo::PendingReceiver<mojom::FastPairDataParser> receiver)
    : receiver_(this, std::move(receiver)) {
  crypto::EnsureOpenSSLInit();
}

FastPairDataParser::~FastPairDataParser() = default;

void FastPairDataParser::GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback) {
  std::move(callback).Run(
      fast_pair_decoder::HasModelId(&service_data)
          ? fast_pair_decoder::GetHexModelIdFromServiceData(&service_data)
          : absl::nullopt);
}

void FastPairDataParser::ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptedResponseCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_response_bytes)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_response_bytes, key, bytes);

  std::move(callback).Run(
      fast_pair_decryption::ParseDecryptedResponse(key, bytes));
}

void FastPairDataParser::ParseDecryptedPasskey(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    ParseDecryptedPasskeyCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_passkey_bytes)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_passkey_bytes, key, bytes);

  std::move(callback).Run(
      fast_pair_decryption::ParseDecryptedPasskey(key, bytes));
}

void CopyFieldBytes(
    const std::vector<uint8_t>& service_data,
    base::flat_map<size_t, std::pair<size_t, size_t>>& field_indices,
    size_t key,
    std::vector<uint8_t>* out) {
  DCHECK(field_indices.contains(key));

  auto indices = field_indices[key];
  for (size_t i = indices.first; i < indices.second; i++) {
    out->push_back(service_data[i]);
  }
}

void FastPairDataParser::ParseNotDiscoverableAdvertisement(
    const std::vector<uint8_t>& service_data,
    ParseNotDiscoverableAdvertisementCallback callback) {
  if (service_data.empty() ||
      fast_pair_decoder::GetVersion(&service_data) != 0) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  base::flat_map<size_t, std::pair<size_t, size_t>> field_indices;
  size_t headerIndex = kHeaderIndex + kHeaderLength +
                       fast_pair_decoder::GetIdLength(&service_data);

  while (headerIndex < service_data.size()) {
    size_t type = service_data[headerIndex] & kFieldTypeBitmask;
    size_t length =
        (service_data[headerIndex] & kFieldLengthBitmask) >> kFieldLengthOffset;
    size_t index = headerIndex + kHeaderLength;
    size_t end = index + length;

    if (end <= service_data.size()) {
      field_indices[type] = std::make_pair(index, end);
    }

    headerIndex = end;
  }

  std::vector<uint8_t> account_key_filter_bytes;
  bool show_ui = false;
  if (field_indices.contains(kFieldTypeAccountKeyFilter)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeAccountKeyFilter,
                   &account_key_filter_bytes);
    show_ui = true;
  } else if (field_indices.contains(kFieldTypeAccountKeyFilterNoNotification)) {
    CopyFieldBytes(service_data, field_indices,
                   kFieldTypeAccountKeyFilterNoNotification,
                   &account_key_filter_bytes);
    show_ui = false;
  }

  std::vector<uint8_t> salt_bytes;
  if (field_indices.contains(kFieldTypeAccountKeyFilterSalt)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeAccountKeyFilterSalt,
                   &salt_bytes);
  }

  std::vector<uint8_t> battery_bytes;
  bool show_ui_for_battery = false;
  if (field_indices.contains(kFieldTypeBattery)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeBattery,
                   &battery_bytes);
    show_ui_for_battery = true;
  } else if (field_indices.contains(kFieldTypeBatteryNoNotification)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeBatteryNoNotification,
                   &battery_bytes);
    show_ui_for_battery = false;
  }

  if (account_key_filter_bytes.empty()) {
    std::move(callback).Run(absl::nullopt);
  } else if (salt_bytes.size() != 1) {
    QP_LOG(WARNING) << "Parsed a salt field larger than one byte: "
                    << salt_bytes.size();
    std::move(callback).Run(absl::nullopt);
  } else {
    std::move(callback).Run(NotDiscoverableAdvertisement(
        std::move(account_key_filter_bytes), show_ui, salt_bytes[0],
        BatteryNotification::FromBytes(battery_bytes, show_ui_for_battery)));
  }
}

}  // namespace quick_pair
}  // namespace ash
