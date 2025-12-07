// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
#define ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {
namespace quick_pair {

class NotificationDelegate;

enum class FastPairNotificationDismissReason {
  kDismissedByUser,
  kDismissedByOs,
  kDismissedByTimeout,
};

// This controller creates and manages a message_center::Notification for each
// FastPair corresponding notification event.
class FastPairNotificationController {
 public:
  explicit FastPairNotificationController(
      message_center::MessageCenter* message_center);
  ~FastPairNotificationController();
  FastPairNotificationController(const FastPairNotificationController&) =
      delete;
  FastPairNotificationController& operator=(
      const FastPairNotificationController&) = delete;

  // Creates and displays corresponding system notification.
  void ShowErrorNotification(
      const std::u16string& device_name,
      gfx::Image device_image,
      base::RepeatingClosure launch_bluetooth_pairing,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowUserDiscoveryNotification(
      const std::u16string& device_name,
      const std::u16string& email_address,
      gfx::Image device_image,
      base::RepeatingClosure on_connect_clicked,
      base::RepeatingClosure on_learn_more_clicked,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowGuestDiscoveryNotification(
      const std::u16string& device_name,
      gfx::Image device_image,
      base::RepeatingClosure on_connect_clicked,
      base::RepeatingClosure on_learn_more_clicked,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowSubsequentDiscoveryNotification(
      const std::u16string& device_name,
      const std::u16string& email_address,
      gfx::Image device_image,
      base::RepeatingClosure on_connect_clicked,
      base::RepeatingClosure on_learn_more_clicked,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowApplicationAvailableNotification(
      const std::u16string& device_name,
      gfx::Image device_image,
      base::RepeatingClosure download_app_callback,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowApplicationInstalledNotification(
      const std::u16string& device_name,
      gfx::Image device_image,
      const std::u16string& app_name,
      base::RepeatingClosure launch_app_callback,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowPairingNotification(
      const std::u16string& device_name,
      gfx::Image device_image,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowAssociateAccount(
      const std::u16string& device_name,
      const std::u16string& email_address,
      gfx::Image device_image,
      base::RepeatingClosure on_save_clicked,
      base::RepeatingClosure on_learn_more_clicked,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close);
  void ShowPasskey(const std::u16string& device_name, uint32_t passkey);
  void RemoveNotifications();
  void ExtendNotification();

 private:
  void RemoveNotificationsByTimeout(
      scoped_refptr<NotificationDelegate> notification_delegate);

  // This timer is used for Discovery and Associate Account notifications to
  // remove them from the Message Center if the user does not elect to begin
  // pairing/saving to their account.
  base::OneShotTimer expire_notification_timer_;

  raw_ptr<message_center::MessageCenter, DanglingUntriaged> message_center_;

  base::WeakPtrFactory<FastPairNotificationController> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
