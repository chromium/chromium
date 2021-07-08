// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/device.h"

#include <ostream>

#include "ash/quick_pair/common/protocol.h"

namespace ash {
namespace quick_pair {

Device::Device(std::string metadata_id, std::string address, Protocol protocol)
    : metadata_id(std::move(metadata_id)),
      address(std::move(address)),
      protocol(protocol) {}

std::ostream& operator<<(std::ostream& stream, const Device& device) {
  stream << "[Device: metadata_id=" << device.metadata_id
         << ", address=" << device.address << ", protocol=" << device.protocol
         << "]";
  return stream;
}

}  // namespace quick_pair
}  // namespace ash
