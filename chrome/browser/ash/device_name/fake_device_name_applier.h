// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_NAME_FAKE_DEVICE_NAME_APPLIER_H_
#define CHROME_BROWSER_ASH_DEVICE_NAME_FAKE_DEVICE_NAME_APPLIER_H_

#include "chrome/browser/ash/device_name/device_name_applier.h"

namespace ash {

// Fake DeviceNameApplier implementation
class FakeDeviceNameApplier : public DeviceNameApplier {
 public:
  FakeDeviceNameApplier();
  ~FakeDeviceNameApplier() override;

  // DeviceNameApplier:
  void SetDeviceName(const std::string& new_device_name) override;

  const std::string& hostname() const { return hostname_; }

 private:
  std::string hostname_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_NAME_FAKE_DEVICE_NAME_APPLIER_H_
