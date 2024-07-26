// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/fixed_array.h"

namespace {

constexpr int kMinModelIdLength = 3;
constexpr int kHeaderIndex = 0;
constexpr int kHeaderLengthBitmask = 0b00011110;
constexpr int kHeaderLengthOffset = 1;
constexpr int kHeaderVersionBitmask = 0b11100000;
constexpr int kHeaderVersionOffset = 5;
constexpr int kMaxModelIdLength = 14;
constexpr int kHeaderLength = 1;

}  // namespace

namespace ash {
namespace quick_pair {
namespace fast_pair_decoder {

int GetVersion(const std::vector<uint8_t>* service_data) {
  return service_data->size() == kMinModelIdLength
             ? 0
             : ((*service_data)[kHeaderIndex] & kHeaderVersionBitmask) >>
                   kHeaderVersionOffset;
}

int GetIdLength(const std::vector<uint8_t>* service_data) {
  return service_data->size() == kMinModelIdLength
             ? kMinModelIdLength
             : ((*service_data)[kHeaderIndex] & kHeaderLengthBitmask) >>
                   kHeaderLengthOffset;
}

bool IsIdLengthValid(const std::vector<uint8_t>* service_data) {
  int id_length = GetIdLength(service_data);
  return kMinModelIdLength <= id_length && id_length <= kMaxModelIdLength &&
         id_length + kHeaderLength <= static_cast<int>(service_data->size());
}

bool HasModelId(const std::vector<uint8_t>* service_data) {
  return service_data != nullptr &&
         (service_data->size() == kMinModelIdLength ||
          // Header byte exists. We support only format version 0. (A different
          // version indicates a breaking change in the format.)
          (service_data->size() > kMinModelIdLength &&
           GetVersion(service_data) == 0 && IsIdLengthValid(service_data)));
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
  // Otherwise, the first byte is a header which contains the length of the
  // big-endian model ID that follows. The model ID will be trimmed if it
  // contains leading zeros.
  int id_index = 1;
  int end = id_index + GetIdLength(service_data);

  // Ignore leading zeros.
  while ((*service_data)[id_index] == 0 && end - id_index > kMinModelIdLength) {
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

}  // namespace fast_pair_decoder
}  // namespace quick_pair
}  // namespace ash
