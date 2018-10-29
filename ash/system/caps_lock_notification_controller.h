// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAPS_LOCK_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_CAPS_LOCK_NOTIFICATION_CONTROLLER_H_

#include "ash/ime/ime_controller.h"
#include "base/macros.h"

class PrefRegistrySimple;

namespace ash {

// Controller class to manage caps lock notification.
class ASH_EXPORT CapsLockNotificationController
    : public ImeController::Observer {
 public:
  CapsLockNotificationController();
  virtual ~CapsLockNotificationController();

  static bool IsSearchKeyMappedToCapsLock();

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CapsLockNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAPS_LOCK_NOTIFICATION_CONTROLLER_H_
