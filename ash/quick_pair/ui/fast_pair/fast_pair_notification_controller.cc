// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/cross_device/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace {

const message_center::NotifierId kNotifierFastPair =
    message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                               "ash.fastpair",
                               ash::NotificationCatalogName::kFastPair);
const char kFastPairErrorNotificationId[] =
    "cros_fast_pair_error_notification_id";
const char kFastPairDiscoveryGuestNotificationId[] =
    "cros_fast_pair_discovery_guest_notification_id";
const char kFastPairApplicationAvailableNotificationId[] =
    "cros_fast_pair_application_available_notification_id";
const char kFastPairApplicationInstalledNotificationId[] =
    "cros_fast_pair_application_installed_notification_id";
const char kFastPairDiscoveryUserNotificationId[] =
    "cros_fast_pair_discovery_user_notification_id";
const char kFastPairPairingNotificationId[] =
    "cros_fast_pair_pairing_notification_id";
const char kFastPairAssociateAccountNotificationId[] =
    "cros_fast_pair_associate_account_notification_id";
const char kFastPairDiscoverySubsequentNotificationId[] =
    "cros_fast_pair_discovery_subsequent_notification_id";
const char kFastPairDisplayPasskeyNotificationId[] =
    "cros_fast_pair_display_passkey_notification_id";

// Values outside of the range (e.g. -1) will show an infinite loading
// progress bar.
const int kInfiniteLoadingProgressValue = -1;

// 12 seconds comes from aligning CrOS notification with Android. Android
// determined it takes 12 seconds for a device to be truly lost to the adapter.
// Within 12 seconds, a device can be perceived as lost, yet be found again.
constexpr base::TimeDelta kNotificationTimeout = base::Seconds(12);

// Creates an empty Fast Pair notification with the given id and uses the
// Bluetooth icon and FastPair notifierID.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    message_center::SystemNotificationWarningLevel warning_level,
    message_center::MessageCenter* message_center) {
  // Remove any existing Fast Pair notifications so only one appears at a time,
  // since there isn't a case where all of them should be showing.
  message_center->RemoveNotificationsForNotifierId(kNotifierFastPair);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
          /*id=*/id,
          /*title=*/std::u16string(),
          /*message=*/std::u16string(),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          /*notifier_id=*/kNotifierFastPair,
          /*optional_fields=*/{},
          /*delegate=*/nullptr,
          /*small_image=*/ash::kNotificationBluetoothIcon,
          /*warning_level=*/warning_level);
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
  explicit NotificationDelegate(
      base::RepeatingClosure on_primary_click,
      base::OnceCallback<void(FastPairNotificationDismissReason)> on_close,
      base::RepeatingClosure on_secondary_click = base::DoNothing(),
      base::OneShotTimer* expire_notification_timer = nullptr) {
    on_primary_click_ = on_primary_click;
    on_secondary_click_ = on_secondary_click;
    on_close_ = std::move(on_close);
    expire_notification_timer_ = expire_notification_timer;
  }

  void DismissedByTimeout() { dismissed_by_timeout_ = true; }

 protected:
  ~NotificationDelegate() override = default;

  // message_center::NotificationDelegate override:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (!button_index)
      return;

    // If the button displayed on the notification is clicked.
    switch (*button_index) {
      case static_cast<int>(Button::kPrimaryButton):
        on_primary_click_.Run();
        break;
      case static_cast<int>(Button::kSecondaryButton):
        on_secondary_click_.Run();
        break;
    }
  }

  // message_center::NotificationDelegate override:
  void Close(bool by_user) override {
    // If there is an expire notification timer, stop the timer if the user
    // dismisses the notification to prevent the timer firing and removing
    // notifications that might come up later.
    if (expire_notification_timer_) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": stopping expiration timer on notification close";
      expire_notification_timer_->Stop();
    }

    if (dismissed_by_timeout_) {
      std::move(on_close_).Run(
          FastPairNotificationDismissReason::kDismissedByTimeout);
      return;
    } else if (by_user) {
      std::move(on_close_).Run(
          FastPairNotificationDismissReason::kDismissedByUser);
      return;
    }

    std::move(on_close_).Run(FastPairNotificationDismissReason::kDismissedByOs);
  }

 private:
  enum class Button { kPrimaryButton, kSecondaryButton };
  bool dismissed_by_timeout_ = false;
  base::RepeatingClosure on_primary_click_;
  base::RepeatingClosure on_secondary_click_;
  base::OnceCallback<void(FastPairNotificationDismissReason)> on_close_;
  raw_ptr<base::OneShotTimer, DanglingUntriaged> expire_notification_timer_;
};

FastPairNotificationController::FastPairNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
}

FastPairNotificationController::~FastPairNotificationController() = default;

void FastPairNotificationController::ShowErrorNotification(
    const std::u16string& device_name,
    gfx::Image device_image,
    base::RepeatingClosure launch_bluetooth_pairing,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  // Because the pairing notification is pinned, we need to manually remove it
  // to explicitly say it is done not by the user. This only need to be done
  // for pinned notifications.
  message_center_->RemoveNotification(kFastPairPairingNotificationId,
                                      /*by_user=*/false);

  std::unique_ptr<message_center::Notification> error_notification =
      CreateNotification(
          kFastPairErrorNotificationId,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
          message_center_);
  error_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_CONNECTION_ERROR_TITLE, device_name));
  error_notification->set_message(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_CONNECTION_ERROR_MESSAGE));

  message_center::ButtonInfo settings_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_SETTINGS_BUTTON));
  error_notification->set_buttons({settings_button});

  error_notification->set_delegate(base::MakeRefCounted<NotificationDelegate>(
      /*on_primary_click=*/launch_bluetooth_pairing,
      /*on_close=*/std::move(on_close)));
  error_notification->SetImage(device_image);

  message_center_->AddNotification(std::move(error_notification));
}

void FastPairNotificationController::ExtendNotification() {
  // If the timer is already running, it implies that there is already a
  // notification being shown for this device. Since the Mediator keeps track
  // of the device for the currently shown notification, we can only get to this
  // point if the notification is for the same device, which means we reset the
  // timeout.
  if (expire_notification_timer_.IsRunning()) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << " extending notification for re-discovered device";
    expire_notification_timer_.Reset();
  }
}

void FastPairNotificationController::ShowUserDiscoveryNotification(
    const std::u16string& device_name,
    const std::u16string& email_address,
    gfx::Image device_image,
    base::RepeatingClosure on_connect_clicked,
    base::RepeatingClosure on_learn_more_clicked,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  std::unique_ptr<message_center::Notification> discovery_notification =
      CreateNotification(kFastPairDiscoveryUserNotificationId,
                         message_center::SystemNotificationWarningLevel::NORMAL,
                         message_center_);
  discovery_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_TITLE, device_name));
  discovery_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_EMAIL_MESSAGE, device_name,
      email_address));

  message_center::ButtonInfo connect_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_CONNECT_BUTTON));
  message_center::ButtonInfo learn_more_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_LEARN_MORE_BUTTON));
  discovery_notification->set_buttons({connect_button, learn_more_button});

  scoped_refptr<NotificationDelegate> notification_delegate =
      base::MakeRefCounted<NotificationDelegate>(
          /*on_primary_click=*/on_connect_clicked,
          /*on_close=*/std::move(on_close),
          /*on_secondary_click=*/on_learn_more_clicked,
          /*expire_notification_timer=*/&expire_notification_timer_);

  discovery_notification->set_delegate(notification_delegate);
  discovery_notification->SetImage(device_image);

  // Start timer for how long to show the notification before removing the
  // notification. After the timeout period, we will remove the notification
  // from the Message Center.
  expire_notification_timer_.Start(
      FROM_HERE, kNotificationTimeout,
      base::BindOnce(
          &FastPairNotificationController::RemoveNotificationsByTimeout,
          weak_ptr_factory_.GetWeakPtr(), notification_delegate));

  message_center_->AddNotification(std::move(discovery_notification));
}

void FastPairNotificationController::ShowGuestDiscoveryNotification(
    const std::u16string& device_name,
    const gfx::Image device_image,
    base::RepeatingClosure on_connect_clicked,
    base::RepeatingClosure on_learn_more_clicked,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  std::unique_ptr<message_center::Notification> discovery_notification =
      CreateNotification(kFastPairDiscoveryGuestNotificationId,
                         message_center::SystemNotificationWarningLevel::NORMAL,
                         message_center_);
  discovery_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_TITLE, device_name));
  discovery_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_MESSAGE, device_name));

  message_center::ButtonInfo connect_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_CONNECT_BUTTON));
  message_center::ButtonInfo learn_more_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_LEARN_MORE_BUTTON));
  discovery_notification->set_buttons({connect_button, learn_more_button});

  scoped_refptr<NotificationDelegate> notification_delegate =
      base::MakeRefCounted<NotificationDelegate>(
          /*on_primary_click=*/on_connect_clicked,
          /*on_close=*/std::move(on_close),
          /*on_secondary_click=*/on_learn_more_clicked,
          /*expire_notification_timer=*/&expire_notification_timer_);

  discovery_notification->set_delegate(notification_delegate);
  discovery_notification->SetImage(device_image);

  // Start timer for how long to show the notification before removing the
  // notification. After the timeout period, we will remove the notification
  // from the Message Center.
  expire_notification_timer_.Start(
      FROM_HERE, kNotificationTimeout,
      base::BindOnce(
          &FastPairNotificationController::RemoveNotificationsByTimeout,
          weak_ptr_factory_.GetWeakPtr(), notification_delegate));

  message_center_->AddNotification(std::move(discovery_notification));
}

void FastPairNotificationController::ShowSubsequentDiscoveryNotification(
    const std::u16string& device_name,
    const std::u16string& email_address,
    gfx::Image device_image,
    base::RepeatingClosure on_connect_clicked,
    base::RepeatingClosure on_learn_more_clicked,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  std::unique_ptr<message_center::Notification> discovery_notification =
      CreateNotification(kFastPairDiscoverySubsequentNotificationId,
                         message_center::SystemNotificationWarningLevel::NORMAL,
                         message_center_);
  discovery_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_TITLE, device_name));
  discovery_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DISCOVERY_NOTIFICATION_SUBSEQUENT, device_name,
      email_address));

  message_center::ButtonInfo connect_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_CONNECT_BUTTON));
  message_center::ButtonInfo learn_more_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_LEARN_MORE_BUTTON));
  discovery_notification->set_buttons({connect_button, learn_more_button});

  scoped_refptr<NotificationDelegate> notification_delegate =
      base::MakeRefCounted<NotificationDelegate>(
          /*on_primary_click=*/on_connect_clicked,
          /*on_close=*/std::move(on_close),
          /*on_secondary_click=*/on_learn_more_clicked,
          /*expire_notification_timer=*/&expire_notification_timer_);

  discovery_notification->set_delegate(notification_delegate);
  discovery_notification->SetImage(device_image);

  // Start timer for how long to show the notification before removing the
  // notification. After the timeout period, we will remove the notification
  // from the Message Center.
  expire_notification_timer_.Start(
      FROM_HERE, kNotificationTimeout,
      base::BindOnce(
          &FastPairNotificationController::RemoveNotificationsByTimeout,
          weak_ptr_factory_.GetWeakPtr(), notification_delegate));

  message_center_->AddNotification(std::move(discovery_notification));
}

void FastPairNotificationController::ShowApplicationAvailableNotification(
    const std::u16string& device_name,
    gfx::Image device_image,
    base::RepeatingClosure download_app_callback,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  std::unique_ptr<message_center::Notification>
      application_available_notification = CreateNotification(
          kFastPairApplicationAvailableNotificationId,
          message_center::SystemNotificationWarningLevel::NORMAL,
          message_center_);
  application_available_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_DOWNLOAD_NOTIFICATION_APP_TITLE, device_name));
  application_available_notification->set_message(l10n_util::GetStringUTF16(
      IDS_FAST_PAIR_DOWNLOAD_NOTIFICATION_APP_MESSAGE));

  message_center::ButtonInfo download_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_DOWNLOAD_APP_BUTTON));
  application_available_notification->set_buttons({download_button});

  application_available_notification->set_delegate(
      base::MakeRefCounted<NotificationDelegate>(
          /*on_primary_click=*/download_app_callback,
          /*on_close=*/std::move(on_close)));
  application_available_notification->SetImage(device_image);

  message_center_->AddNotification(
      std::move(application_available_notification));
}

void FastPairNotificationController::ShowApplicationInstalledNotification(
    const std::u16string& device_name,
    gfx::Image device_image,
    const std::u16string& app_name,
    base::RepeatingClosure launch_app_callback,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  std::unique_ptr<message_center::Notification>
      application_installed_notification = CreateNotification(
          kFastPairApplicationInstalledNotificationId,
          message_center::SystemNotificationWarningLevel::NORMAL,
          message_center_);
  application_installed_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_SETUP_APP_NOTIFICATION_TITLE, device_name));
  application_installed_notification->set_message(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_SETUP_APP_NOTIFICATION_MESSAGE));

  message_center::ButtonInfo setup_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_SETUP_APP_BUTTON));
  application_installed_notification->set_buttons({setup_button});

  application_installed_notification->set_delegate(
      base::MakeRefCounted<NotificationDelegate>(
          /*on_primary_click=*/launch_app_callback,
          /*on_close=*/std::move(on_close)));
  application_installed_notification->SetImage(device_image);

  message_center_->AddNotification(
      std::move(application_installed_notification));
}

void FastPairNotificationController::ShowPairingNotification(
    const std::u16string& device_name,
    gfx::Image device_image,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  // If we get to this point in the pairing flow where we are showing the
  // Pairing notification, then the user has elected to begin pairing and we
  // can stop the timer that was waiting for user interaction on the
  // Discovery notification. We do not need the timer for the Pairing
  // notification since it will be removed when pairing succeeds or fails by
  // the system.
  expire_notification_timer_.Stop();

  std::unique_ptr<message_center::Notification> pairing_notification =
      CreateNotification(kFastPairPairingNotificationId,
                         message_center::SystemNotificationWarningLevel::NORMAL,
                         message_center_);
  pairing_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_PAIRING_NOTIFICATION_TITLE, device_name));

  pairing_notification->set_delegate(base::MakeRefCounted<NotificationDelegate>(
      /*on_primary_click=*/base::DoNothing(),
      /*on_close=*/std::move(on_close)));
  pairing_notification->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  pairing_notification->set_progress(kInfiniteLoadingProgressValue);
  pairing_notification->SetImage(device_image);
  pairing_notification->set_pinned(true);

  message_center_->AddNotification(std::move(pairing_notification));
}

void FastPairNotificationController::ShowAssociateAccount(
    const std::u16string& device_name,
    const std::u16string& email_address,
    gfx::Image device_image,
    base::RepeatingClosure on_save_clicked,
    base::RepeatingClosure on_learn_more_clicked,
    base::OnceCallback<void(FastPairNotificationDismissReason)> on_close) {
  std::unique_ptr<message_center::Notification> associate_account_notification =
      CreateNotification(kFastPairAssociateAccountNotificationId,
                         message_center::SystemNotificationWarningLevel::NORMAL,
                         message_center_);
  associate_account_notification->set_title(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_ASSOCIATE_ACCOUNT_NOTIFICATION_TITLE, device_name));
  associate_account_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_FAST_PAIR_ASSOCIATE_ACCOUNT_NOTIFICATION_MESSAGE, device_name,
      email_address));

  message_center::ButtonInfo save_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_SAVE_BUTTON));
  message_center::ButtonInfo learn_more_button(
      l10n_util::GetStringUTF16(IDS_FAST_PAIR_LEARN_MORE_BUTTON));
  associate_account_notification->set_buttons({save_button, learn_more_button});

  scoped_refptr<NotificationDelegate> notification_delegate =
      base::MakeRefCounted<NotificationDelegate>(
          /*on_primary_click=*/on_save_clicked,
          /*on_close=*/std::move(on_close),
          /*on_secondary_click=*/on_learn_more_clicked,
          /*expire_notification_timer=*/&expire_notification_timer_);

  associate_account_notification->set_delegate(notification_delegate);
  associate_account_notification->SetImage(device_image);

  // Start timer for how long to show the notification before removing the
  // notification. After the timeout period, we will remove the notification
  // from the Message Center.
  expire_notification_timer_.Start(
      FROM_HERE, kNotificationTimeout,
      base::BindOnce(
          &FastPairNotificationController::RemoveNotificationsByTimeout,
          weak_ptr_factory_.GetWeakPtr(), notification_delegate));

  message_center_->AddNotification(std::move(associate_account_notification));
}

void FastPairNotificationController::ShowPasskey(
    const std::u16string& device_name,
    uint32_t passkey) {
  std::unique_ptr<message_center::Notification> passkey_notification =
      CreateNotification(kFastPairDisplayPasskeyNotificationId,
                         message_center::SystemNotificationWarningLevel::NORMAL,
                         message_center_);
  passkey_notification->set_message(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_DISPLAY_PASSKEY, device_name,
      base::UTF8ToUTF16(base::StringPrintf("%06i", passkey))));

  message_center_->AddNotification(std::move(passkey_notification));
}

void FastPairNotificationController::RemoveNotifications() {
  message_center_->RemoveNotificationsForNotifierId(kNotifierFastPair);
}

void FastPairNotificationController::RemoveNotificationsByTimeout(
    scoped_refptr<NotificationDelegate> notification_delegate) {
  notification_delegate->DismissedByTimeout();
  RemoveNotifications();
}

}  // namespace quick_pair
}  // namespace ash
