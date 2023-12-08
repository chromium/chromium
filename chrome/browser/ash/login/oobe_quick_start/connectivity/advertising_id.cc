// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"

#include "base/base64url.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "crypto/random.h"

namespace ash::quick_start {

// static
std::optional<AdvertisingId> AdvertisingId::ParseFromBase64(
    const std::string& encoded_advertising_id) {
  std::string decoded_output;

  if (!base::Base64UrlDecode(encoded_advertising_id,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_output)) {
    QS_LOG(ERROR)
        << "Failed to decode the advertising ID. Encoded advertising ID: "
        << encoded_advertising_id;
    return std::nullopt;
  }

  if (decoded_output.length() != kLength) {
    QS_LOG(ERROR) << "Decoded advertising ID is an unexpected length. "
                     "Decoded advertising ID output: "
                  << decoded_output;
    return std::nullopt;
  }

  std::array<uint8_t, kLength> bytes;

  for (size_t i = 0; i < decoded_output.length(); i++) {
    bytes[i] = uint8_t(decoded_output[i]);
  }

  return AdvertisingId(std::move(bytes));
}

AdvertisingId::AdvertisingId() {
  crypto::RandBytes(bytes_);
}

AdvertisingId::AdvertisingId(base::span<const uint8_t, kLength> bytes) {
  base::ranges::copy(bytes, bytes_.begin());
}

std::string AdvertisingId::ToString() const {
  std::string advertising_id_bytes(bytes_.begin(), bytes_.end());
  std::string advertising_id_base64;
  base::Base64UrlEncode(advertising_id_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &advertising_id_base64);
  return advertising_id_base64;
}

std::string AdvertisingId::GetDisplayCode() const {
  uint32_t high = bytes_[0];
  uint32_t low = bytes_[1];
  uint32_t x = (high << 8) + low;
  return base::StringPrintf("%03d", x % 1000);
}

std::ostream& operator<<(std::ostream& stream,
                         const AdvertisingId& advertising_id) {
  return stream << advertising_id.ToString();
}

}  // namespace ash::quick_start
