// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
const char kNotifierUsbPeripheral[] = "ash.usb_peripheral";
const char kUsbPeripheralInvalidDpCableNotificationId[] =
    "cros_usb_peripheral_invalid_dp_cable_notification_id";
const char kUsbPeripheralInvalidUSB4ValidTBTCableNotificationId[] =
    "cros_usb_peripheral_invalid_usb4_valid_tbt_cable_notification_id";
const char kUsbPeripheralInvalidUSB4CableNotificationId[] =
    "cros_usb_peripheral_invalid_usb4_cable_notification_id";
const char kUsbPeripheralInvalidTBTCableNotificationId[] =
    "cros_usb_peripheral_invalid_tbt_cable_notification_id";
const char kUsbPeripheralSpeedLimitingCableNotificationId[] =
    "cros_usb_peripheral_speed_limiting_cable_notification_id";
const char kNotificationDisplayLandingPageUrl[] =
    "https://support.google.com/chromebook?p=cable_notification";
const char kNotificationDeviceLandingPageUrl[] =
    "https://support.google.com/chromebook?p=cable_notification_2";

bool GetCableSpeedNotificationShownPref() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  return prefs->GetBoolean(prefs::kUsbPeripheralCableSpeedNotificationShown);
}

void SetCableSpeedNotificationShownPref(bool pref) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  prefs->SetBoolean(prefs::kUsbPeripheralCableSpeedNotificationShown, pref);
}

bool ShouldDisplayNotification() {
  return Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

void OnCableNotificationClicked(const std::string& notification_id,
                                const std::string& landing_page,
                                std::optional<int> button_index) {
  if (notification_id == kUsbPeripheralSpeedLimitingCableNotificationId)
    SetCableSpeedNotificationShownPref(true);

  if (button_index) {
    NewWindowDelegate::GetInstance()->OpenUrl(
        GURL(landing_page), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
        NewWindowDelegate::Disposition::kNewForegroundTab);
  }

  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*from_user=*/true);
}

}  // namespace

UsbPeripheralNotificationController::UsbPeripheralNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
}

UsbPeripheralNotificationController::~UsbPeripheralNotificationController() {
  if (ash::PeripheralNotificationManager::IsInitialized())
    ash::PeripheralNotificationManager::Get()->RemoveObserver(this);
}

// static
void UsbPeripheralNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kUsbPeripheralCableSpeedNotificationShown, false);
}

void UsbPeripheralNotificationController::
    OnPeripheralNotificationManagerInitialized() {
  DCHECK(ash::PeripheralNotificationManager::IsInitialized());
  ash::PeripheralNotificationManager::Get()->AddObserver(this);
}

// Notify the user that the current cable may not support dp alt mode.
void UsbPeripheralNotificationController::OnInvalidDpCableWarning() {
  if (!ShouldDisplayNotification())
    return;

  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_USB_NOTIFICATION_V2_LEARN_MORE)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUsbPeripheralInvalidDpCableNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_DISPLAY_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_DISPLAY_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierUsbPeripheral,
              NotificationCatalogName::kUSBPeripheralInvalidDpCable),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnCableNotificationClicked,
                                  kUsbPeripheralInvalidDpCableNotificationId,
                                  kNotificationDisplayLandingPageUrl)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

// Notify the user that the USB4 device will use TBT due to the cable.
void UsbPeripheralNotificationController::OnInvalidUSB4ValidTBTCableWarning() {
  if (!ShouldDisplayNotification())
    return;

  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_USB_NOTIFICATION_V2_LEARN_MORE)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUsbPeripheralInvalidUSB4ValidTBTCableNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_PERFORMANCE_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_NO_USB4_SUPPORT_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierUsbPeripheral,
              NotificationCatalogName::kUSBPeripheralInvalidUSB4ValidTBTCable),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &OnCableNotificationClicked,
                  kUsbPeripheralInvalidUSB4ValidTBTCableNotificationId,
                  kNotificationDeviceLandingPageUrl)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

// Notify the user that the USB4 device will use DisplayPort,
// USB 3.2 or USB 2.0 due to the cable.
void UsbPeripheralNotificationController::OnInvalidUSB4CableWarning() {
  if (!ShouldDisplayNotification())
    return;

  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_USB_NOTIFICATION_V2_LEARN_MORE)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUsbPeripheralInvalidUSB4CableNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_PERFORMANCE_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_NO_USB4_SUPPORT_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierUsbPeripheral,
              NotificationCatalogName::kUSBPeripheralInvalidUSB4Cable),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnCableNotificationClicked,
                                  kUsbPeripheralInvalidUSB4CableNotificationId,
                                  kNotificationDeviceLandingPageUrl)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

// Notify the user that the TBT device will use DisplayPort,
// USB 3.2 or USB 2.0 due to the cable.
void UsbPeripheralNotificationController::OnInvalidTBTCableWarning() {
  if (!ShouldDisplayNotification())
    return;

  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_USB_NOTIFICATION_V2_LEARN_MORE)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUsbPeripheralInvalidTBTCableNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_PERFORMANCE_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_NO_TBT_SUPPORT_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierUsbPeripheral,
              NotificationCatalogName::kUSBPeripheralInvalidTBTCable),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnCableNotificationClicked,
                                  kUsbPeripheralInvalidTBTCableNotificationId,
                                  kNotificationDeviceLandingPageUrl)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

// Notify the user that the cable limits USB device performance.
void UsbPeripheralNotificationController::OnSpeedLimitingCableWarning() {
  if (!ShouldDisplayNotification() || GetCableSpeedNotificationShownPref())
    return;

  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_USB_NOTIFICATION_V2_LEARN_MORE)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUsbPeripheralSpeedLimitingCableNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_PERFORMANCE_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_USB_NOTIFICATION_V2_CABLE_WARNING_SPEED_LIMITED_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierUsbPeripheral,
              NotificationCatalogName::kUSBPeripheralSpeedLimitingCable),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &OnCableNotificationClicked,
                  kUsbPeripheralSpeedLimitingCableNotificationId,
                  kNotificationDeviceLandingPageUrl)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
