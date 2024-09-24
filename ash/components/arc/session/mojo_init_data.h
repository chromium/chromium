// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_MOJO_INIT_DATA_H_
#define ASH_COMPONENTS_ARC_SESSION_MOJO_INIT_DATA_H_

#include <fcntl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/token.h"

namespace arc {

// Contains data to be sent to ARC during bootstrap connection.
class MojoInitData {
 public:
  struct InterfaceVersion {
    base::Token uuid;
    uint32_t version;

    // Comparison as a tuple of (`uuid`, `version`).
    constexpr bool operator<(const InterfaceVersion& other) const;
  };

  MojoInitData();
  MojoInitData(const MojoInitData&) = delete;
  MojoInitData& operator=(const MojoInitData&) = delete;

  ~MojoInitData();

  // Returns a vector containing the pointer to each data.
  // Note that the returned value is no longer available after `MojoInitData`
  // being destroyed.
  //
  // Protocol version 0:
  //  Send version, token_length, and token.
  // Protocol version 1:
  //  Send version, token_length, and token.
  //  After that, send the number of interfaces and the list of
  //  {uuid, version} for each interface.
  std::vector<iovec> AsIOvecVector();

  const std::string& token() const { return token_; }

 private:
  const uint8_t protocol_version_;

  // Token is sent at the beginning of the communication and used as the name of
  // the message pipe.
  const std::string token_;
};

}  // namespace arc
#endif  // ASH_COMPONENTS_ARC_SESSION_MOJO_INIT_DATA_H_
