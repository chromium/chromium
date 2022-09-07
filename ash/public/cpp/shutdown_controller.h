// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHUTDOWN_CONTROLLER_H_
#define ASH_PUBLIC_CPP_SHUTDOWN_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scoped_singleton_resetter_for_test.h"

namespace ash {

enum class ShutdownReason;

// Interface implemented by ash and used by chrome to provide shutdown policy
// information. Exists because device policy is owned by chrome, not ash.
class ASH_PUBLIC_EXPORT ShutdownController {
 public:
  using ScopedResetterForTest =
      ScopedSingletonResetterForTest<ShutdownController>;

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
