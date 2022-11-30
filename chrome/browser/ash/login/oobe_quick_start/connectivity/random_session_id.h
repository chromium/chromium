// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_RANDOM_SESSION_ID_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_RANDOM_SESSION_ID_H_

#include <array>
#include <ostream>

#include "base/containers/span.h"

namespace ash::quick_start {

// An immutable, copyable type representing ten random bytes.
class RandomSessionId {
 public:
  // This length is chosen to be 10 bytes in order to match the format used by
  // SmartSetup on Android for interoperability.
  static constexpr size_t kLength = 10;

  RandomSessionId();
  explicit RandomSessionId(base::span<const uint8_t, kLength> bytes);
  RandomSessionId(RandomSessionId&) = default;
  RandomSessionId& operator=(RandomSessionId&) = default;
  ~RandomSessionId() = default;

  base::span<const uint8_t, kLength> AsBytes() const { return bytes_; }

  // Convert to hexadecimal.
  std::string ToString() const;

 private:
  std::array<uint8_t, kLength> bytes_;
};

// Write the RandomSessionId to the ostream in hexadecimal for logging.
std::ostream& operator<<(std::ostream& stream,
                         const RandomSessionId& random_session_id);

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_RANDOM_SESSION_ID_H_
