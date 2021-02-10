// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_DEFAULT_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_DEFAULT_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"

namespace chromeos {
namespace system {

class DeviceDisablingManagerDefaultDelegate
    : public DeviceDisablingManager::Delegate {
 public:
  DeviceDisablingManagerDefaultDelegate();

 private:
  // DeviceDisablingManager::Delegate:
  void RestartToLoginScreen() override;
  void ShowDeviceDisabledScreen() override;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManagerDefaultDelegate);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_DEFAULT_DELEGATE_H_
