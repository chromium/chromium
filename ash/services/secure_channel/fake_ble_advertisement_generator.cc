// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/secure_channel/fake_ble_advertisement_generator.h"

#include "ash/components/multidevice/remote_device_ref.h"

namespace ash::secure_channel {

FakeBleAdvertisementGenerator::FakeBleAdvertisementGenerator() {}

FakeBleAdvertisementGenerator::~FakeBleAdvertisementGenerator() {}

std::unique_ptr<DataWithTimestamp>
FakeBleAdvertisementGenerator::GenerateBleAdvertisementInternal(
    multidevice::RemoteDeviceRef remote_device,
    const std::string& local_device_public_key) {
  return std::move(advertisement_);
}

}  // namespace ash::secure_channel
