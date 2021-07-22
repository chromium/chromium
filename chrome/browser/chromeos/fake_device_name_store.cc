// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fake_device_name_store.h"

namespace chromeos {

// static
const char FakeDeviceNameStore::kDefaultDeviceName[] = "ChromeOS";

FakeDeviceNameStore::FakeDeviceNameStore() {
  device_name_ = kDefaultDeviceName;
}

FakeDeviceNameStore::~FakeDeviceNameStore() = default;

std::string FakeDeviceNameStore::GetDeviceName() const {
  return device_name_;
}

void FakeDeviceNameStore::SetDeviceName(const std::string& new_device_name) {
  device_name_ = new_device_name;
}

}  // namespace chromeos
