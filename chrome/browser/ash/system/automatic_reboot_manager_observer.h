// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_AUTOMATIC_REBOOT_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_ASH_SYSTEM_AUTOMATIC_REBOOT_MANAGER_OBSERVER_H_

namespace ash {
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
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_AUTOMATIC_REBOOT_MANAGER_OBSERVER_H_
