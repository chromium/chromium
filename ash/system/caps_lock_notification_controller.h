// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAPS_LOCK_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_CAPS_LOCK_NOTIFICATION_CONTROLLER_H_

#include "ash/ime/ime_controller_impl.h"

class PrefRegistrySimple;

namespace ash {

// Controller class to manage caps lock notification.
class ASH_EXPORT CapsLockNotificationController
    : public ImeController::Observer {
 public:
  CapsLockNotificationController();

  CapsLockNotificationController(const CapsLockNotificationController&) =
      delete;
  CapsLockNotificationController& operator=(
      const CapsLockNotificationController&) = delete;

  virtual ~CapsLockNotificationController();

  static bool IsSearchKeyMappedToCapsLock();

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAPS_LOCK_NOTIFICATION_CONTROLLER_H_
