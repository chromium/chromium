// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata.h"

namespace ash {
namespace quick_pair {

DeviceMetadata::DeviceMetadata(const nearby::fastpair::Device device,
                               const gfx::Image image)
    : device(std::move(device)), image(std::move(image)) {}

}  // namespace quick_pair
}  // namespace ash
