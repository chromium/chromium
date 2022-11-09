// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace quick_pair {

// Thin wrapper around decoded metadata for a Fast Pair device.
class DeviceMetadata {
 public:
  DeviceMetadata(const nearby::fastpair::GetObservedDeviceResponse device,
                 const gfx::Image image);
  DeviceMetadata(DeviceMetadata&&);
  DeviceMetadata(const DeviceMetadata&) = delete;
  DeviceMetadata& operator=(const DeviceMetadata&) = delete;
  DeviceMetadata& operator=(DeviceMetadata&&) = delete;
  ~DeviceMetadata();

  const nearby::fastpair::Device& GetDetails();
  DeviceFastPairVersion InferFastPairVersion();
  const gfx::Image& image() { return image_; }
  const nearby::fastpair::GetObservedDeviceResponse& response() {
    return response_;
  }

 private:
  const nearby::fastpair::GetObservedDeviceResponse response_;
  const gfx::Image image_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_H_
