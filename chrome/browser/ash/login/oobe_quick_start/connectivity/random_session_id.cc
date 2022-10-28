// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/random.h"

namespace ash::quick_start {

RandomSessionId::RandomSessionId() {
  crypto::RandBytes(bytes_);
}

RandomSessionId::RandomSessionId(base::span<const uint8_t, kLength> bytes) {
  base::ranges::copy(bytes, bytes_.begin());
}

std::string RandomSessionId::ToString() const {
  return base::HexEncode(bytes_);
}

std::ostream& operator<<(std::ostream& stream,
                         const RandomSessionId& random_session_id) {
  return stream << random_session_id.ToString();
}

}  // namespace ash::quick_start
