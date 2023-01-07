// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_BASE_H_
#define ASH_PUBLIC_CPP_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_BASE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Public interface to manage power button behavior.
class ASH_PUBLIC_EXPORT PowerButtonControllerBase {
 public:
  static PowerButtonControllerBase* Get();

  // Handles events from ARC++ to open power button menu. As this is not called
  // from physical power button, it might have different behavior, e.g. not
  // need to run pre-shutdown.
  virtual void OnArcPowerButtonMenuEvent() = 0;

  // Cancels the ongoing power button behavior. This can be called while the
  // button is still held to prevent any action from being taken on release.
  virtual void CancelPowerButtonEvent() = 0;

 protected:
  PowerButtonControllerBase();
  virtual ~PowerButtonControllerBase();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_BASE_H_
