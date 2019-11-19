// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHUTDOWN_CONTROLLER_H_
#define ASH_PUBLIC_CPP_SHUTDOWN_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum class ShutdownReason;

// Interface implemented by ash and used by chrome to provide shutdown policy
// information. Exists because device policy is owned by chrome, not ash.
class ASH_PUBLIC_EXPORT ShutdownController {
 public:
  // Helper class to reset ShutdowController instance in constructor and
  // restore it in destructor so that tests could create its own instance.
  class ScopedResetterForTest {
   public:
    ScopedResetterForTest();
    ~ScopedResetterForTest();

   private:
    ShutdownController* const instance_;
  };

  // Gets the singleton ShutdownController instance.
  static ShutdownController* Get();

  // Sets a boolean pref that indicates whether the device automatically reboots
  // when the user initiates a shutdown via an UI element. Used in enterprise
  // environments for devices that should not be shut down.
  virtual void SetRebootOnShutdown(bool reboot_on_shutdown) = 0;

  // Shuts down or reboots based on the current DeviceRebootOnShutdown policy.
  // Does not trigger the shutdown fade-out animation. For animated shutdown
  // use LockStateController::RequestShutdown().
  virtual void ShutDownOrReboot(ShutdownReason reason) = 0;

 protected:
  ShutdownController();
  virtual ~ShutdownController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHUTDOWN_CONTROLLER_H_
