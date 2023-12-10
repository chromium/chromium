// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/device.h"

#include <ostream>

#include "ash/quick_pair/common/protocol.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"

namespace {

std::ostream& OutputToStream(std::ostream& stream,
                             const std::string& metadata_id,
                             const std::string& ble_address,
                             const std::optional<std::string>& classic_address,
                             const std::optional<std::string>& display_name,
                             const ash::quick_pair::Protocol& protocol) {
  stream << "[Device: metadata_id=" << metadata_id;

  // We can only include PII from the device in verbose logging.
  if (VLOG_IS_ON(/*verbose_level=*/1)) {
    stream << ", ble_address=" << ble_address
           << ", classic_address=" << classic_address.value_or("null")
           << ", display_name=" << display_name.value_or("null");
  }

  stream << ", protocol=" << protocol << "]";
  return stream;
}

}  // namespace

namespace ash {
namespace quick_pair {

Device::Device(const std::string& metadata_id,
               const std::string& ble_address,
               Protocol protocol)
    : metadata_id_(metadata_id),
      ble_address_(ble_address),
      protocol_(protocol) {}

Device::~Device() = default;

std::ostream& operator<<(std::ostream& stream, const Device& device) {
  return OutputToStream(stream, device.metadata_id(), device.ble_address(),
                        device.classic_address(), device.display_name(),
                        device.protocol());
}

std::ostream& operator<<(std::ostream& stream, scoped_refptr<Device> device) {
  return OutputToStream(stream, device->metadata_id(), device->ble_address(),
                        device->classic_address(), device->display_name(),
                        device->protocol());
}

}  // namespace quick_pair
}  // namespace ash
