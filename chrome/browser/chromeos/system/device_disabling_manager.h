// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace policy {
class BrowserPolicyConnectorChromeOS;
}

namespace user_manager {
class UserManager;
}

namespace chromeos {
namespace system {

// If an enrolled device is lost or stolen, it can be remotely disabled by its
// owner. The disabling is triggered in two different ways, depending on the
// state the device is in:
// - If the device has been wiped, it will perform a hash dance during OOBE to
//   find out whether any persistent state has been stored for it on the server.
//   If so, persistent state is retrieved as a |DeviceStateRetrievalResponse|
//   protobuf, parsed and written to the |prefs::kServerBackedDeviceState| local
//   state pref. At the appropriate place in the OOBE flow, the
//   |WizardController| will call CheckWhetherDeviceDisabledDuringOOBE() to find
//   out whether the device is disabled, causing it to either show or skip the
//   device disabled screen.
// - If the device has not been wiped, the disabled state is retrieved with
//   every device policy fetch as part of the |PolicyData| protobuf, parsed and
//   written to the |chromeos::kDeviceDisabled| cros setting. This class
//   monitors the cros setting. When the device becomes disabled, one of two
//   actions is taken:
//   1) If no session is in progress, the device disabled screen is shown
//      immediately.
//   2) If a session is in progress, the session is terminated. After Chrome has
//      restarted on the login screen, the disabled screen is shown per 1).
//   This ensures that when a device is disabled, there is never any user
//   session running in the backround.
//   When the device is re-enabled, Chrome is restarted once more to resume the
//   regular login screen flows from a known-good point.
class DeviceDisablingManager {
 public:
  using DeviceDisabledCheckCallback = base::Callback<void(bool)>;

  class Observer {
   public:
    virtual ~Observer();

    virtual void OnDisabledMessageChanged(
        const std::string& disabled_message) = 0;

   private:
    DISALLOW_ASSIGN(Observer);
  };

  class Delegate {
   public:
    virtual ~Delegate();

    // Terminate the current session (if any) and restart Chrome to show the
    // login screen.
    virtual void RestartToLoginScreen() = 0;

    // Show the device disabled screen.
    virtual void ShowDeviceDisabledScreen() = 0;

   private:
    DISALLOW_ASSIGN(Delegate);
  };

  // |delegate| must outlive |this|.
  DeviceDisablingManager(Delegate* delegate,
                         CrosSettings* cros_settings,
                         user_manager::UserManager* user_manager);
  ~DeviceDisablingManager();

  // Must be called after construction.
  void Init();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the cached domain that owns the device. The domain is only
  // guaranteed to be up to date if the disabled screen was triggered.
  const std::string& enrollment_domain() const { return enrollment_domain_; }

  // Returns the cached disabled message. The message is only guaranteed to be
  // up to date if the disabled screen was triggered.
  const std::string& disabled_message() const { return disabled_message_; }

  // Returns the cached serial_number. The value is only guaranteed to be
  // up to date if the disabled screen was triggered.
  const std::string& serial_number() const { return serial_number_; }

  // Performs a check whether the device is disabled during OOBE. |callback|
  // will be invoked with the result of the check.
  void CheckWhetherDeviceDisabledDuringOOBE(
      const DeviceDisabledCheckCallback& callback);

  // Returns true if trusted cros settings say the device is disabled and the
  // device disabled setting should be honored.
  static bool IsDeviceDisabledDuringNormalOperation();

  // Whenever trusted cros settings indicate that the device is disabled, this
  // method should be used to check whether the device disabling is to be
  // honored. If this method returns false, the device should not be disabled.
  static bool HonorDeviceDisablingDuringNormalOperation();

 private:
  // Cache the disabled message and inform observers if it changed.
  void CacheDisabledMessageAndNotify(const std::string& disabled_message);

  void UpdateFromCrosSettings();

  Delegate* delegate_;
  policy::BrowserPolicyConnectorChromeOS* browser_policy_connector_;
  CrosSettings* cros_settings_;
  user_manager::UserManager* user_manager_;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      device_disabled_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      disabled_message_subscription_;

  // Indicates whether the device was disabled when the cros settings were last
  // read.
  bool device_disabled_;

  // A cached copy of the domain that owns the device.
  std::string enrollment_domain_;

  // A cached copy of the message to show on the device disabled screen.
  std::string disabled_message_;

  // A cached copy of the serial number to show on the device disabled screen.
  std::string serial_number_;

  base::WeakPtrFactory<DeviceDisablingManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManager);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_H_
