// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"

#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
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
const int kNotificationsClicksThreshold = 3;

const char kPciePeripheralLimitedPerformanceNotificationId[] =
    "cros_pcie_peripheral_limited_performance_notification_id";
const char kPciePeripheralLimitedPerformanceGuestModeNotificationId[] =
    "cros_pcie_peripheral_limited_performance_guest_mode_notification_id";
const char kPciePeripheralGuestModeNotSupportedNotificationId[] =
    "cros_pcie_peripheral_guest_mode_not_supported_notifcation_id";
const char kPciePeripheralDeviceBlockedNotificationId[] =
    "cros_pcie_peripheral_device_blocked_notifcation_id";
const char kPciePeripheralBillboardDeviceNotificationId[] =
    "cros_pcie_peripheral_billboard_device_notifcation_id";

// Represents the buttons in the notification.
enum ButtonIndex { kSettings, kLearnMore };

int GetNotificationClickPrefCount() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  return prefs->GetInteger(prefs::kPciePeripheralDisplayNotificationRemaining);
}

void UpdateNotificationPrefCount(bool clicked_settings) {
  int current_pref_val = GetNotificationClickPrefCount();

  // We're already not showing any new notifications, don't update.
  if (current_pref_val == 0)
    return;

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  // If the user has reached the settings page through the notification, do
  // not show any more new notifications.
  if (clicked_settings) {
    prefs->SetInteger(prefs::kPciePeripheralDisplayNotificationRemaining, 0);
    return;
  }

  // Otherwise, decrement the pref count.
  prefs->SetInteger(prefs::kPciePeripheralDisplayNotificationRemaining,
                    current_pref_val - 1);
}

void ShowPrivacyAndSecuritySettings() {
  Shell::Get()->system_tray_model()->client()->ShowPrivacyAndSecuritySettings();
}

void RemoveNotification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*from_user=*/true);
}

void OnPeripheralLimitedNotificationClicked(std::optional<int> button_index) {
  // Clicked on body.
  if (!button_index) {
    ShowPrivacyAndSecuritySettings();
    UpdateNotificationPrefCount(/*clicked_settings=*/true);
    RemoveNotification(kPciePeripheralLimitedPerformanceNotificationId);
    return;
  }

  switch (*button_index) {
    case ButtonIndex::kSettings:
      ShowPrivacyAndSecuritySettings();
      UpdateNotificationPrefCount(/*clicked_settings=*/true);
      break;
    case ButtonIndex::kLearnMore:
      NewWindowDelegate::GetPrimary()->OpenUrl(
          GURL(kLearnMoreHelpUrl),
          NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          NewWindowDelegate::Disposition::kNewForegroundTab);
      break;
  }
  RemoveNotification(kPciePeripheralLimitedPerformanceNotificationId);
}

void OnGuestNotificationClicked(bool is_thunderbolt_only) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kLearnMoreHelpUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);

  if (is_thunderbolt_only) {
    RemoveNotification(kPciePeripheralGuestModeNotSupportedNotificationId);
    return;
  }

  RemoveNotification(kPciePeripheralLimitedPerformanceGuestModeNotificationId);
}

void OnPeripheralBlockedNotificationClicked() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kLearnMoreHelpUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  RemoveNotification(kPciePeripheralDeviceBlockedNotificationId);
}

void OnBillboardNotificationClicked() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kLearnMoreHelpUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  RemoveNotification(kPciePeripheralBillboardDeviceNotificationId);
}

// We only display notifications for active user sessions (signed-in/guest with
// desktop ready). Also do not show notifications in signin or lock screen.
bool ShouldDisplayNotification() {
  return Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

}  // namespace

PciePeripheralNotificationController::PciePeripheralNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
}

PciePeripheralNotificationController::~PciePeripheralNotificationController() {
  if (ash::PeripheralNotificationManager::IsInitialized())
    ash::PeripheralNotificationManager::Get()->RemoveObserver(this);
}

void PciePeripheralNotificationController::
    OnPeripheralNotificationManagerInitialized() {
  DCHECK(ash::PeripheralNotificationManager::IsInitialized());

  ash::PeripheralNotificationManager::Get()->AddObserver(this);
}

void PciePeripheralNotificationController::NotifyBillboardDevice() {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kPciePeripheralBillboardDeviceNotificationId,
          /*title=*/std::u16string(),
          l10n_util::GetStringUTF16(
              IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_BILLBOARD_DEVICE),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierPciePeripheral,
              NotificationCatalogName::kPcieBillboardDevice),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnBillboardNotificationClicked)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  message_center_->AddNotification(std::move(notification));
}

void PciePeripheralNotificationController::NotifyLimitedPerformance() {
  // Don't show the notification if the user has already clicked on the
  // notification three times.
  if (!ShouldDisplayNotification() || GetNotificationClickPrefCount() == 0)
    return;

  message_center::RichNotificationData optional;
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_SETTINGS_BUTTON_TEXT)));
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_LEARN_MORE_BUTTON_TEXT)));

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kPciePeripheralLimitedPerformanceNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_PERFORMANCE_LIMITED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_PERFORMANCE_LIMITED_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierPciePeripheral,
              NotificationCatalogName::kPcieLimitedPerformance),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnPeripheralLimitedNotificationClicked)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
  UpdateNotificationPrefCount(/*clicked_settings=*/false);
}

void PciePeripheralNotificationController::NotifyGuestModeNotification(
    bool is_thunderbolt_only) {
  if (!ShouldDisplayNotification())
    return;

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          is_thunderbolt_only
              ? kPciePeripheralGuestModeNotSupportedNotificationId
              : kPciePeripheralLimitedPerformanceGuestModeNotificationId,
          /*title=*/std::u16string(),
          is_thunderbolt_only
              ? l10n_util::GetStringUTF16(
                    IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_GUEST_MODE_NOT_SUPPORTED)
              : l10n_util::GetStringUTF16(
                    IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_PERFORMANCE_LIMITED_GUEST_MODE),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierPciePeripheral, NotificationCatalogName::kPcieGuestMode),
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

void PciePeripheralNotificationController::
    NotifyPeripheralBlockedNotification() {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kPciePeripheralDeviceBlockedNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_DEVICE_BLOCKED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_PCIE_PERIPHERAL_NOTIFICATION_DEVICE_BLOCKED_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierPciePeripheral,
              NotificationCatalogName::kPciePeripheralBlocked),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnPeripheralBlockedNotificationClicked)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  message_center_->AddNotification(std::move(notification));
}

void PciePeripheralNotificationController::
    OnLimitedPerformancePeripheralReceived() {
  NotifyLimitedPerformance();
}

void PciePeripheralNotificationController::OnGuestModeNotificationReceived(
    bool is_thunderbolt_only) {
  NotifyGuestModeNotification(is_thunderbolt_only);
}

void PciePeripheralNotificationController::OnPeripheralBlockedReceived() {
  NotifyPeripheralBlockedNotification();
}

void PciePeripheralNotificationController::OnBillboardDeviceConnected() {
  NotifyBillboardDevice();
}

// static
void PciePeripheralNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // By default, we let the user click on the notifications three times before
  // hiding future notifications.
  registry->RegisterIntegerPref(
      prefs::kPciePeripheralDisplayNotificationRemaining,
      kNotificationsClicksThreshold);
}
}  // namespace ash
