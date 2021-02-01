// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_AUTOMATIC_REBOOT_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_AUTOMATIC_REBOOT_MANAGER_OBSERVER_H_

namespace chromeos {
namespace system {

class AutomaticRebootManagerObserver {
 public:
  enum Reason {
    REBOOT_REASON_UNKNOWN,
    REBOOT_REASON_OS_UPDATE,
    REBOOT_REASON_PERIODIC,
  };

  // Invoked when a reboot is requested.
  virtual void OnRebootRequested(Reason reason) = 0;

  // Invoked before the automatic reboot manager is destroyed.
  virtual void WillDestroyAutomaticRebootManager() = 0;

 protected:
  virtual ~AutomaticRebootManagerObserver() {}
};

}  // namespace system
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to chrome/browser/ash/.
namespace ash {
namespace system {
using ::chromeos::system::AutomaticRebootManagerObserver;
}
}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_AUTOMATIC_REBOOT_MANAGER_OBSERVER_H_
