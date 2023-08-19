// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"

#include "base/base64url.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "crypto/random.h"

namespace ash::quick_start {

// static
absl::optional<RandomSessionId> RandomSessionId::ParseFromBase64(
    const std::string& encoded_random_session_id) {
  std::string decoded_output;

  if (!base::Base64UrlDecode(encoded_random_session_id,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_output)) {
    QS_LOG(ERROR)
        << "Failed to decode the random session ID. Encoded random session ID: "
        << encoded_random_session_id;
    return absl::nullopt;
  }

  if (decoded_output.length() != kLength) {
    QS_LOG(ERROR) << "Decoded random session ID is an unexpected length. "
                     "Decoded random session ID output: "
                  << decoded_output;
    return absl::nullopt;
  }

  std::array<uint8_t, kLength> bytes;

  for (size_t i = 0; i < decoded_output.length(); i++) {
    bytes[i] = uint8_t(decoded_output[i]);
  }

  return RandomSessionId(std::move(bytes));
}

RandomSessionId::RandomSessionId() {
  crypto::RandBytes(bytes_);
}

RandomSessionId::RandomSessionId(base::span<const uint8_t, kLength> bytes) {
  base::ranges::copy(bytes, bytes_.begin());
}

std::string RandomSessionId::ToString() const {
  std::string session_id_bytes(bytes_.begin(), bytes_.end());
  std::string session_id_base64;
  base::Base64UrlEncode(session_id_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &session_id_base64);
  return session_id_base64;
}

std::string RandomSessionId::GetDisplayCode() const {
  uint32_t high = bytes_[0];
  uint32_t low = bytes_[1];
  uint32_t x = (high << 8) + low;
  return base::StringPrintf("%03d", x % 1000);
}

std::ostream& operator<<(std::ostream& stream,
                         const RandomSessionId& random_session_id) {
  return stream << random_session_id.ToString();
}

}  // namespace ash::quick_start
