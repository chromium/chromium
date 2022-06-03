// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata.h"

namespace ash {
namespace quick_pair {

DeviceMetadata::DeviceMetadata(
    const nearby::fastpair::GetObservedDeviceResponse response,
    const gfx::Image image)
    : response_(std::move(response)), image_(std::move(image)) {}

DeviceMetadata::DeviceMetadata(DeviceMetadata&&) = default;

DeviceMetadata::~DeviceMetadata() = default;

const nearby::fastpair::Device& DeviceMetadata::GetDetails() {
  return response_.device();
}

}  // namespace quick_pair
}  // namespace ash
