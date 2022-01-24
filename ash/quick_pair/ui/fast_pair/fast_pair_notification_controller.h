// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
#define ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace quick_pair {

// This controller creates and manages a message_center::Notification for each
// FastPair corresponding notification event.
class COMPONENT_EXPORT(QUICK_PAIR_UI) FastPairNotificationController {
 public:
  FastPairNotificationController();
  ~FastPairNotificationController();
  FastPairNotificationController(const FastPairNotificationController&) =
      delete;
  FastPairNotificationController& operator=(
      const FastPairNotificationController&) = delete;

  // Creates and displays corresponding system notification.
  void ShowErrorNotification(const std::u16string& device_name,
                             gfx::Image device_image,
                             base::RepeatingClosure launch_bluetooth_pairing,
                             base::OnceCallback<void(bool)> on_close);
  void ShowDiscoveryNotification(const std::u16string& device_name,
                                 gfx::Image device_image,
                                 base::RepeatingClosure on_connect_clicked,
                                 base::OnceCallback<void(bool)> on_close);
  void ShowPairingNotification(const std::u16string& device_name,
                               gfx::Image device_image,
                               base::RepeatingClosure on_cancel_clicked,
                               base::OnceCallback<void(bool)> on_close);
  void ShowAssociateAccount(const std::u16string& device_name,
                            const std::u16string& email_address,
                            gfx::Image device_image,
                            base::RepeatingClosure on_save_clicked,
                            base::RepeatingClosure on_learn_more_clicked,
                            base::OnceCallback<void(bool)> on_close);
  void RemoveNotifications();
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
