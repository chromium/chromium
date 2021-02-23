// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
const char kNotifierPciePeripheral[] = "ash.pcie_peripheral";
const char kLearnMoreHelpUrl[] =
    "https://www.support.google.com/chromebook?p=connect_thblt_usb4_accy";

const char kPciePeripheralLimitedPerformanceNotificationId[] =
    "cros_pcie_peripheral_limited_performance_notification_id";
const char kPciePeripheralLimitedPerformanceGuestModeNotificationId[] =
    "cros_pcie_peripheral_limited_performance_guest_mode_notification_id";
const char kPciePeripheralGuestModeNotSupportedNotificationId[] =
    "cros_pcie_peripheral_guest_mode_not_supported_notifcation_id";

// Represents the buttons in the notification.
enum ButtonIndex { kSettings, kLearnMore };

void ShowPrivacyAndSecuritySettings() {
  Shell::Get()->system_tray_model()->client()->ShowPrivacyAndSecuritySettings();
}

void RemoveNotification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*from_user=*/true);
}

void OnPeripheralLimitedNotificationClicked(base::Optional<int> button_index) {
  // Clicked on body.
  if (!button_index) {
    ShowPrivacyAndSecuritySettings();
    RemoveNotification(kPciePeripheralLimitedPerformanceNotificationId);
    return;
  }

  switch (*button_index) {
    case ButtonIndex::kSettings:
      ShowPrivacyAndSecuritySettings();
      break;
    case ButtonIndex::kLearnMore:
      NewWindowDelegate::GetInstance()->NewTabWithUrl(
          GURL(kLearnMoreHelpUrl), /*from_user_interaction=*/true);
      break;
  }
  RemoveNotification(kPciePeripheralLimitedPerformanceNotificationId);
}

void OnGuestNotificationClicked(bool is_thunderbolt_only) {
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kLearnMoreHelpUrl), /*from_user_interaction=*/true);

  if (is_thunderbolt_only) {
    RemoveNotification(kPciePeripheralGuestModeNotSupportedNotificationId);
    return;
  }

  RemoveNotification(kPciePeripheralLimitedPerformanceGuestModeNotificationId);
}

}  // namespace

PciePeripheralNotificationController::PciePeripheralNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
}

PciePeripheralNotificationController::~PciePeripheralNotificationController() =
    default;

void PciePeripheralNotificationController::NotifyLimitedPerformance() {
  message_center::RichNotificationData optional;
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_PCIE_PERIPHERAL_NOTIFICATION_SETTINGS_BUTTON_TEXT)));
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_PCIE_PERIPHERAL_NOTIFICATION_LEARN_MORE_BUTTON_TEXT)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kPciePeripheralLimitedPerformanceNotificationId,
          l10n_util::GetStringUTF16(
              IDS_PCIE_PERIPHERAL_NOTIFICATION_PERFORMANCE_LIMITED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_PCIE_PERIPHERAL_NOTIFICATION_PERFORMANCE_LIMITED_BODY),
          /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierPciePeripheral),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnPeripheralLimitedNotificationClicked)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

void PciePeripheralNotificationController::NotifyGuestModeNotification(
    bool is_thunderbolt_only) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          is_thunderbolt_only
              ? kPciePeripheralGuestModeNotSupportedNotificationId
              : kPciePeripheralLimitedPerformanceGuestModeNotificationId,
          /*title=*/base::string16(),
          is_thunderbolt_only
              ? l10n_util::GetStringUTF16(
                    IDS_PCIE_PERIPHERAL_NOTIFICATION_GUEST_MODE_NOT_SUPPORTED)
              : l10n_util::GetStringUTF16(
                    IDS_PCIE_PERIPHERAL_NOTIFICATION_PERFORMANCE_LIMITED_GUEST_MODE),
          /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierPciePeripheral),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnGuestNotificationClicked,
                                  is_thunderbolt_only)),
          kSettingsIcon,
          is_thunderbolt_only
              ? message_center::SystemNotificationWarningLevel::CRITICAL_WARNING
              : message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
