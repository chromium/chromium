// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_H_

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/component_export.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace quick_pair {

// Thin wrapper around decoded metadata for a Fast Pair device.
struct COMPONENT_EXPORT(QUICK_PAIR_REPOSITORY) DeviceMetadata {
  DeviceMetadata(const nearby::fastpair::Device device, const gfx::Image image);
  DeviceMetadata(const DeviceMetadata&) = delete;
  DeviceMetadata(DeviceMetadata&&);
  DeviceMetadata& operator=(const DeviceMetadata&) = delete;
  DeviceMetadata& operator=(DeviceMetadata&&) = delete;
  ~DeviceMetadata() = default;

  const nearby::fastpair::Device device;
  const gfx::Image image;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_METADATA_H_
