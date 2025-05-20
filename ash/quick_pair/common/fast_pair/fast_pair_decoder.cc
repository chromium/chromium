// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"

#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/fixed_array.h"

namespace {

constexpr int kMinModelIdLength = 3;
constexpr int kHeaderIndex = 0;

// Format 2018 (0bVVVLLLLR)
constexpr int kHeaderLengthBitmask2018 = 0b00011110;
constexpr int kHeaderLengthOffset2018 = 1;
constexpr int kHeaderVersionBitmask2018 = 0b11100000;
constexpr int kHeaderVersionOffset2018 = 5;
constexpr int kMaxModelIdLength = 14;
const std::string k2018HeaderPrefix = "06";

// Format 2022 (0bVVVVFFFF)
constexpr int kHeaderVersionBitmask2022 = 0b11110000;
constexpr int kHeaderVersionOffset2022 = 4;
constexpr int kHeaderFlagsBitmask2022 = 0b00001111;

constexpr int kHeaderLength = 1;
constexpr int kExtraFieldHeaderLength = 1;
constexpr int kExtraFieldLengthBitmask = 0b11110000;
constexpr int kExtraFieldLengthOffset = 4;
constexpr int kExtraFieldTypeBitmask = 0b00001111;
constexpr int kExtraFieldTypeOffset = 0;
constexpr int kExtraFieldTypeModelId = 7;

}  // namespace

namespace ash {
namespace quick_pair {
namespace fast_pair_decoder {

int GetVersion(const std::vector<uint8_t>* service_data) {
  if (!features::IsFastPairAdvertisingFormat2025Enabled()) {
    return service_data->size() == kMinModelIdLength
               ? 0
               : GetVersion2018(service_data);
  }

  // If feature flag enabled, only handle 2022 format advertisements.
  return service_data->size() == kMinModelIdLength
             ? 0
             : GetVersion2022(service_data);
}

/** For format 2018, get version from the first 3 bits (VVVLLLLR) */
int GetVersion2018(const std::vector<uint8_t>* service_data) {
  return ((*service_data)[kHeaderIndex] & kHeaderVersionBitmask2018) >>
         kHeaderVersionOffset2018;
}

/** For format 2022, get version from the first 4 bits (VVVVFFFF) */
int GetVersion2022(const std::vector<uint8_t>* service_data) {
  return ((*service_data)[kHeaderIndex] & kHeaderVersionBitmask2022) >>
         kHeaderVersionOffset2022;
}

int GetFlags(const std::vector<uint8_t>* service_data) {
  CHECK(features::IsFastPairAdvertisingFormat2025Enabled());

  return (*service_data)[kHeaderIndex] & kHeaderFlagsBitmask2022;
}

int GetExtraFieldLength(const std::vector<uint8_t>* service_data, int index) {
  return ((*service_data)[index] & kExtraFieldLengthBitmask) >>
         kExtraFieldLengthOffset;
}

int GetExtraFieldType(const std::vector<uint8_t>* service_data, int index) {
  return ((*service_data)[index] & kExtraFieldTypeBitmask) >>
         kExtraFieldTypeOffset;
}

// TODO(399163998): Remove deprecated code after feature launch.
int GetIdLength(const std::vector<uint8_t>* service_data) {
  CHECK(!features::IsFastPairAdvertisingFormat2025Enabled());

  return service_data->size() == kMinModelIdLength
             ? kMinModelIdLength
             : ((*service_data)[kHeaderIndex] & kHeaderLengthBitmask2018) >>
                   kHeaderLengthOffset2018;
}

// TODO(399163998): Remove deprecated code after feature launch.
bool IsIdLengthValid(const std::vector<uint8_t>* service_data) {
  CHECK(!features::IsFastPairAdvertisingFormat2025Enabled());

  int id_length = GetIdLength(service_data);
  return kMinModelIdLength <= id_length && id_length <= kMaxModelIdLength &&
         id_length + kHeaderLength <= static_cast<int>(service_data->size());
}

// TODO(399163998): Remove deprecated code after feature launch.
bool HasModelId(const std::vector<uint8_t>* service_data) {
  CHECK(!features::IsFastPairAdvertisingFormat2025Enabled());

  return service_data != nullptr &&
         (service_data->size() == kMinModelIdLength ||
          // Header byte exists. We support only format version 0. (A different
          // version indicates a breaking change in the format.)
          (service_data->size() > kMinModelIdLength &&
           GetVersion(service_data) == 0 && IsIdLengthValid(service_data)));
}

// Returns hex-encoded string of field with type |extra_field_type|, or
// std::nullopt if no match was found or fields are improperly formatted.
std::optional<std::string> GetExtraField(
    const std::vector<uint8_t>* service_data,
    int extra_field_type) {
  // Iterate through extra fields, which have the following format (including
  // overall header): Header (0bVVVVFFFF) LT V LT V
  // ...

  size_t headerIndex = kHeaderLength;
  while (headerIndex < service_data->size()) {
    size_t length = GetExtraFieldLength(service_data, headerIndex);
    int type = GetExtraFieldType(service_data, headerIndex);

    size_t start = headerIndex + kExtraFieldHeaderLength;
    size_t end = start + length;
    if (length < 1 || end > service_data->size()) {
      LOG(ERROR) << __func__ << ": Improper length for extra fields, aborting.";
      return std::nullopt;
    }

    if (type == extra_field_type) {
      // found it!
      // Extract extra field to new vector.
      std::vector<uint8_t> extra_field(length);

      for (size_t i = 0; i < length; i++) {
        extra_field[i] = (*service_data)[i + start];
      }
      return base::HexEncode(extra_field);
    }

    headerIndex = end;
  }

  LOG(ERROR) << __func__ << ": Extra field type not found.";
  return std::nullopt;
}

std::optional<std::string> GetHexModelIdFromServiceData(
    const std::vector<uint8_t>* service_data) {
  if (service_data == nullptr || service_data->size() < kMinModelIdLength) {
    return std::nullopt;
  }
  if (service_data->size() == kMinModelIdLength) {
    // If the size is 3, all the bytes are the ID,
    return base::HexEncode(*service_data);
  }

  // The following logic is used only to support the deprecated 2018 format.
  if (!features::IsFastPairAdvertisingFormat2025Enabled()) {
    // Otherwise, the first byte is a header which contains the length of the
    // big-endian model ID that follows. The model ID will be trimmed if it
    // contains leading zeros.
    int id_index = 1;
    int end = id_index + GetIdLength(service_data);

    // Ignore leading zeros.
    while ((*service_data)[id_index] == 0 &&
           end - id_index > kMinModelIdLength) {
      id_index++;
    }

    // Copy appropriate bytes to new array.
    int bytes_size = end - id_index;
    base::FixedArray<uint8_t> bytes(bytes_size);

    for (int i = 0; i < bytes_size; i++) {
      bytes[i] = (*service_data)[i + id_index];
    }

    return base::HexEncode(base::span<uint8_t>(bytes));
  }

  std::string service_data_str = base::HexEncode(*service_data);
  if (service_data_str.starts_with(k2018HeaderPrefix)) {
    LOG(WARNING) << __func__
                 << ": Ignoring deprecated 2018 advertising format.";
    return std::nullopt;
  }

  // As per go/spec_audio_sharing_payload, the 1st byte will be 00 for devices
  // that support LE audio sharing (introduced 2025).
  if (GetVersion(service_data) == 0 && GetFlags(service_data) == 0) {
    return GetExtraField(service_data, kExtraFieldTypeModelId);
  }

  return std::nullopt;
}

}  // namespace fast_pair_decoder
}  // namespace quick_pair
}  // namespace ash
