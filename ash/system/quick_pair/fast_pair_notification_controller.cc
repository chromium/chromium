// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/quick_pair/fast_pair_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kNotifierFastPair[] = "ash.fastpair";
const char kFastPairErrorNotificationId[] =
    "cros_fast_pair_error_notification_id";

}  // namespace

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
  // Add 'Settings' button to the notification that will trigger the
  // NotificationDelegate 'Click' method which will run the on_click callback.
  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo settings_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_SETTINGS_BUTTON));
  optional_fields.buttons.push_back(settings_button);

  std::unique_ptr<message_center::Notification> error_notification =
      CreateSystemNotification(
          /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
          /*id=*/kFastPairErrorNotificationId,
          /*title=*/
          l10n_util::GetStringFUTF16(IDS_FAST_PAIR_CONNECTION_ERROR_TITLE,
                                     device_name),
          /*message=*/
          l10n_util::GetStringFUTF16(IDS_FAST_PAIR_CONNECTION_ERROR_MESSAGE,
                                     std::u16string()),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          /*notifier_id=*/
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierFastPair),
          optional_fields,
          /*delegate=*/
          base::MakeRefCounted<NotificationDelegate>(
              std::move(launch_bluetooth_pairing), std::move(on_close)),
          /*small_image=*/kNotificationBluetoothIcon,
          /*warning_level=*/
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  error_notification->set_image(device_image);
  error_notification->set_priority(
      message_center::NotificationPriority::MAX_PRIORITY);

  MessageCenter::Get()->AddNotification(std::move(error_notification));
}

void FastPairNotificationController::ShowDiscoveryNotification() {}

void FastPairNotificationController::ShowPairingNotification() {}

}  // namespace ash
