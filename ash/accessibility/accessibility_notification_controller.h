// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_NOTIFICATION_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/functional/callback.h"

namespace ash {

// Class that manages showing notifications for accessibility.
class ASH_EXPORT AccessibilityNotificationController {
 public:
  AccessibilityNotificationController();
  AccessibilityNotificationController(
      const AccessibilityNotificationController&) = delete;
  AccessibilityNotificationController& operator=(
      const AccessibilityNotificationController&) = delete;
  ~AccessibilityNotificationController();

  void ShowToast(AccessibilityToastType type);
  void AddShowToastCallbackForTesting(
      base::RepeatingCallback<void(AccessibilityToastType)> callback);

 private:
  base::RepeatingCallback<void(AccessibilityToastType)>
      show_anchored_nudge_callback_for_testing_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_NOTIFICATION_CONTROLLER_H_
