// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FAKE_DEVICE_NAME_STORE_H_
#define CHROME_BROWSER_CHROMEOS_FAKE_DEVICE_NAME_STORE_H_

#include "chrome/browser/chromeos/device_name_store.h"

namespace chromeos {

// Fake DeviceNameStore implementation
class FakeDeviceNameStore : public DeviceNameStore {
 public:
  static const char kDefaultDeviceName[];

  FakeDeviceNameStore();
  ~FakeDeviceNameStore() override;

  // Sets the result of setting the device name the next time SetDeviceName() is
  // called.
  void SetNextSetDeviceNameResult(
      DeviceNameStore::SetDeviceNameResult name_update_result);

  // DeviceNameStore:
  std::string GetDeviceName() const override;
  DeviceNameStore::SetDeviceNameResult SetDeviceName(
      const std::string& new_device_name) override;

 private:
  std::string device_name_ = kDefaultDeviceName;
  DeviceNameStore::SetDeviceNameResult name_update_result_ =
      DeviceNameStore::SetDeviceNameResult::kSuccess;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FAKE_DEVICE_NAME_STORE_H_
