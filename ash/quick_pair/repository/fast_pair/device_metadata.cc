// Copyright 2021 The Chromium Authors
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

DeviceFastPairVersion DeviceMetadata::InferFastPairVersion() {
  // Anti-spoofing keys were introduced in Fast Pair v2, so if this isn't
  // available then the device is v1.
  if (GetDetails().anti_spoofing_key_pair().public_key().empty()) {
    return DeviceFastPairVersion::kV1;
  }
  return DeviceFastPairVersion::kHigherThanV1;
}

}  // namespace quick_pair
}  // namespace ash
