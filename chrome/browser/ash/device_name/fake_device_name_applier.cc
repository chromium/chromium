// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/fake_device_name_applier.h"

namespace ash {

FakeDeviceNameApplier::FakeDeviceNameApplier() = default;

FakeDeviceNameApplier::~FakeDeviceNameApplier() = default;

void FakeDeviceNameApplier::SetDeviceName(const std::string& new_device_name) {
  hostname_ = new_device_name;
}

}  // namespace ash
