// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"

#include "base/base64url.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "crypto/random.h"

namespace ash::quick_start {

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
