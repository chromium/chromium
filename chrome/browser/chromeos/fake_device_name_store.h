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

  // DeviceNameStore:
  std::string GetDeviceName() const override;

  void SetDeviceName(const std::string& new_device_name);

 private:
  std::string device_name_ = kDefaultDeviceName;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FAKE_DEVICE_NAME_STORE_H_
