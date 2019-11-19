// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"

namespace chromeos {

namespace system {
class DeviceDisablingManager;
}

class DeviceDisabledScreenView;

// Screen informing the user that the device has been disabled by its owner.
class DeviceDisabledScreen : public BaseScreen,
                             public system::DeviceDisablingManager::Observer {
 public:
  explicit DeviceDisabledScreen(DeviceDisabledScreenView* view);
  ~DeviceDisabledScreen() override;

  // Called when the view is being destroyed. Note that if the Delegate is
  // destroyed first, it must call SetDelegate(nullptr).
  void OnViewDestroyed(DeviceDisabledScreenView* view);

  // Returns the domain that owns the device.
  const std::string& GetEnrollmentDomain() const;

  // Returns the message that should be shown to the user.
  const std::string& GetMessage() const;

  // Returns the device serial number that should be shown to the user.
  const std::string& GetSerialNumber() const;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // system::DeviceDisablingManager::Observer:
  void OnDisabledMessageChanged(const std::string& disabled_message) override;

 private:
  DeviceDisabledScreenView* view_;

  // Whether the screen is currently showing.
  bool showing_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisabledScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
