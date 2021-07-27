// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fake_device_name_store.h"

namespace chromeos {

// static
const char FakeDeviceNameStore::kDefaultDeviceName[] = "ChromeOS";

FakeDeviceNameStore::FakeDeviceNameStore() = default;

FakeDeviceNameStore::~FakeDeviceNameStore() = default;

std::string FakeDeviceNameStore::GetDeviceName() const {
  return device_name_;
}

DeviceNameStore::SetDeviceNameResult FakeDeviceNameStore::SetDeviceName(
    const std::string& new_device_name) {
  if (name_update_result_ == DeviceNameStore::SetDeviceNameResult::kSuccess)
    device_name_ = new_device_name;
  return name_update_result_;
}

void FakeDeviceNameStore::SetNextSetDeviceNameResult(
    DeviceNameStore::SetDeviceNameResult name_update_result) {
  name_update_result_ = name_update_result;
}

}  // namespace chromeos
