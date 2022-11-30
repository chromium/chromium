// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/fake_device_name_store.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ash/device_name/device_name_validator.h"

namespace ash {

// static
const char FakeDeviceNameStore::kDefaultDeviceName[] = "ChromeOS";

FakeDeviceNameStore::FakeDeviceNameStore() = default;

FakeDeviceNameStore::~FakeDeviceNameStore() = default;

DeviceNameStore::DeviceNameMetadata FakeDeviceNameStore::GetDeviceNameMetadata()
    const {
  return {device_name_, device_name_state_};
}

DeviceNameStore::SetDeviceNameResult FakeDeviceNameStore::SetDeviceName(
    const std::string& new_device_name) {
  switch (device_name_state_) {
    case DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy:
      return DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy;

    case DeviceNameStore::DeviceNameState::
        kCannotBeModifiedBecauseNotDeviceOwner:
      return DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner;

    case DeviceNameStore::DeviceNameState::kCanBeModified:
      if (!IsValidDeviceName(new_device_name))
        return DeviceNameStore::SetDeviceNameResult::kInvalidName;

      device_name_ = new_device_name;
      NotifyDeviceNameMetadataChanged();
      return DeviceNameStore::SetDeviceNameResult::kSuccess;
  }
}

void FakeDeviceNameStore::SetDeviceNameState(
    DeviceNameStore::DeviceNameState device_name_state) {
  if (device_name_state_ == device_name_state)
    return;

  device_name_state_ = device_name_state;
  NotifyDeviceNameMetadataChanged();
}

}  // namespace ash
