// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_QUICK_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_QUICK_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

// This controller creates and manages a message_center::Notification for each
// FastPair corresponding notification.
class ASH_EXPORT FastPairNotificationController {
 public:
  FastPairNotificationController();
  ~FastPairNotificationController();
  FastPairNotificationController(const FastPairNotificationController&) =
      delete;
  FastPairNotificationController& operator=(
      const FastPairNotificationController&) = delete;

 private:
  // Creates and displays corresponding system notification.
  void ShowErrorNotification();
  void ShowDiscoveryNotification();
  void ShowPairingNotification();
};

}  // namespace ash

#endif  // ASH_SYSTEM_QUICK_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
