// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_DEVICE_DISABLING_MANAGER_DEFAULT_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_DEVICE_DISABLING_MANAGER_DEFAULT_DELEGATE_H_

#include "chrome/browser/ash/system/device_disabling_manager.h"

namespace ash {
namespace system {

class DeviceDisablingManagerDefaultDelegate
    : public DeviceDisablingManager::Delegate {
 public:
  DeviceDisablingManagerDefaultDelegate();

  DeviceDisablingManagerDefaultDelegate(
      const DeviceDisablingManagerDefaultDelegate&) = delete;
  DeviceDisablingManagerDefaultDelegate& operator=(
      const DeviceDisablingManagerDefaultDelegate&) = delete;

 private:
  // DeviceDisablingManager::Delegate:
  void RestartToLoginScreen() override;
  void ShowDeviceDisabledScreen() override;
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_DEVICE_DISABLING_MANAGER_DEFAULT_DELEGATE_H_
