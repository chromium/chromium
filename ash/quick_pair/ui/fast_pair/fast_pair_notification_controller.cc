// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace {

const char kNotifierFastPair[] = "ash.fastpair";
const char kFastPairErrorNotificationId[] =
    "cros_fast_pair_error_notification_id";
const char kFastPairDiscoveryNotificationId[] =
    "cros_fast_pair_discovery_notification_id";
const char kFastPairPairingNotificationId[] =
    "cros_fast_pair_pairing_notification_id";

// Values outside of the range (e.g. -1) will show an infinite loading
// progress bar.
const int kInfiniteLoadingProgressValue = -1;

// Creates an empty Fast Pair notification with the given id and uses the
// Bluetooth icon and FastPair notifierID.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
          /*id=*/id,
          /*title=*/std::u16string(),
          /*message=*/std::u16string(),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          /*notifier_id=*/
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierFastPair),
          /*optional_fields=*/{},
          /*delegate=*/nullptr,
          /*small_image=*/ash::kNotificationBluetoothIcon,
          /*warning_level=*/
          message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_never_timeout(true);
  notification->set_priority(
      message_center::NotificationPriority::MAX_PRIORITY);

  return notification;
}

}  // namespace

namespace ash {
namespace quick_pair {

// NotificationDelegate implementation for handling click and dismiss events on
// a notification. Used by Error, Pairing, and Discovery notifications.
class NotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit NotificationDelegate(base::OnceClosure on_click,
                                base::OnceCallback<void(bool)> on_close) {
    on_click_ = std::move(on_click);
    on_close_ = std::move(on_close);
  }

 protected:
  ~NotificationDelegate() override = default;

  // message_center::NotificationDelegate override:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override {
    // If the button displayed on the notification is clicked.
    if (button_index) {
      std::move(on_click_).Run();
    }
  }

  // message_center::NotificationDelegate override:
  void Close(bool by_user) override { std::move(on_close_).Run(by_user); }

 private:
  base::OnceClosure on_click_;
  base::OnceCallback<void(bool)> on_close_;
};

FastPairNotificationController::FastPairNotificationController() = default;

FastPairNotificationController::~FastPairNotificationController() = default;

void FastPairNotificationController::ShowErrorNotification(
    const std::u16string& device_name,
    gfx::Image device_image,
    base::OnceClosure launch_bluetooth_pairing,
    base::OnceCallback<void(bool)> on_close) {
  std::unique_ptr<message_center::Notification> error_notification =
      CreateNotification(kFastPairErrorNotificationId);
  error_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_CONNECTION_ERROR_TITLE, device_name));
  error_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_CONNECTION_ERROR_MESSAGE, std::u16string()));

  message_center::ButtonInfo settings_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_SETTINGS_BUTTON));
  error_notification->set_buttons({settings_button});

  error_notification->set_delegate(base::MakeRefCounted<NotificationDelegate>(
      std::move(launch_bluetooth_pairing), std::move(on_close)));
  error_notification->set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  error_notification->set_image(device_image);

  MessageCenter::Get()->AddNotification(std::move(error_notification));
}

void FastPairNotificationController::ShowDiscoveryNotification(
    const std::u16string& device_name,
    const gfx::Image device_image,
    base::OnceClosure on_connect_clicked,
    base::OnceCallback<void(bool)> on_close) {
  std::unique_ptr<message_center::Notification> discovery_notification =
      CreateNotification(kFastPairDiscoveryNotificationId);
  discovery_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_TITLE, device_name));
  discovery_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_MESSAGE, std::u16string()));

  message_center::ButtonInfo connect_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_CONNECT_BUTTON));
  discovery_notification->set_buttons({connect_button});

  discovery_notification->set_delegate(
      base::MakeRefCounted<NotificationDelegate>(std::move(on_connect_clicked),
                                                 std::move(on_close)));
  discovery_notification->set_image(device_image);

  MessageCenter::Get()->AddNotification(std::move(discovery_notification));
}

void FastPairNotificationController::ShowPairingNotification(
    const std::u16string& device_name,
    gfx::Image device_image,
    base::OnceClosure on_cancel_clicked,
    base::OnceCallback<void(bool)> on_close) {
  std::unique_ptr<message_center::Notification> pairing_notification =
      CreateNotification(kFastPairPairingNotificationId);
  pairing_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_PAIRING_NOTIFICATION_TITLE, device_name));

  message_center::ButtonInfo cancel_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_CANCEL_BUTTON));
  pairing_notification->set_buttons({cancel_button});

  pairing_notification->set_delegate(base::MakeRefCounted<NotificationDelegate>(
      std::move(on_cancel_clicked), std::move(on_close)));
  pairing_notification->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  pairing_notification->set_progress(kInfiniteLoadingProgressValue);
  pairing_notification->set_image(device_image);

  MessageCenter::Get()->AddNotification(std::move(pairing_notification));
}

}  // namespace quick_pair
}  // namespace ash
