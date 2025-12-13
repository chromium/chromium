// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/adb_sideloading_policy_change_notification.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"

namespace {
constexpr char kAdbSideloadingDisallowedNotificationId[] =
    "chrome://adb_sideloading_disallowed";
constexpr char kAdbSideloadingPowerwashPlannedNotificationId[] =
    "chrome://adb_sideloading_powerwash_planned";
constexpr char kAdbSideloadingPowerwashOnRebootNotificationId[] =
    "chrome://adb_sideloading_powerwash_on_reboot";
}  // namespace

namespace ash {

AdbSideloadingPolicyChangeNotification::AdbSideloadingPolicyChangeNotification(
    const policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash)
    : browser_policy_connector_ash_(CHECK_DEREF(browser_policy_connector_ash)) {
}
AdbSideloadingPolicyChangeNotification::
    ~AdbSideloadingPolicyChangeNotification() = default;

void AdbSideloadingPolicyChangeNotification::Show(Type type) {
  std::u16string title, text;
  std::string notification_id;
  NotificationCatalogName catalog_name;
  bool pinned = false;
  std::vector<message_center::ButtonInfo> notification_actions;

  auto enterprise_manager = base::UTF8ToUTF16(
      browser_policy_connector_ash_->GetEnterpriseDomainManager());
  std::u16string device_type = ui::GetChromeOSDeviceName();

  switch (type) {
    case Type::kNone:
      NOTREACHED();
    case Type::kSideloadingDisallowed:
      title = l10n_util::GetStringUTF16(
          IDS_ADB_SIDELOADING_POLICY_CHANGE_SIDELOADING_DISALLOWED_NOTIFICATION_TITLE);
      text = l10n_util::GetStringFUTF16(
          IDS_ADB_SIDELOADING_POLICY_CHANGE_SIDELOADING_DISALLOWED_NOTIFICATION_MESSAGE,
          enterprise_manager, device_type);
      notification_id = kAdbSideloadingDisallowedNotificationId;
      catalog_name = NotificationCatalogName::kAdbSideloadingDisallowed;
      break;
    case Type::kPowerwashPlanned:
      title = l10n_util::GetStringFUTF16(
          IDS_ADB_SIDELOADING_POLICY_CHANGE_POWERWASH_PLANNED_NOTIFICATION_TITLE,
          device_type);
      text = l10n_util::GetStringFUTF16(
          IDS_ADB_SIDELOADING_POLICY_CHANGE_POWERWASH_PLANNED_NOTIFICATION_MESSAGE,
          enterprise_manager, device_type);
      notification_id = kAdbSideloadingPowerwashPlannedNotificationId;
      catalog_name = NotificationCatalogName::kAdbSideloadingPowerwashPlanned;
      break;
    case Type::kPowerwashOnNextReboot:
      title = l10n_util::GetStringFUTF16(
          IDS_ADB_SIDELOADING_POLICY_CHANGE_POWERWASH_ON_REBOOT_NOTIFICATION_TITLE,
          device_type);
      text = l10n_util::GetStringFUTF16(
          IDS_ADB_SIDELOADING_POLICY_CHANGE_POWERWASH_ON_REBOOT_NOTIFICATION_MESSAGE,
          enterprise_manager, device_type);
      notification_id = kAdbSideloadingPowerwashOnRebootNotificationId;
      catalog_name = NotificationCatalogName::kAdbSideloadingPowerwashOnReboot;
      pinned = true;
      notification_actions.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_ADB_SIDELOADING_POLICY_CHANGE_RESTART_TO_POWERWASH)));
      break;
  }

  auto notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, text,
      std::u16string() /*display_source*/, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id, catalog_name),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &AdbSideloadingPolicyChangeNotification::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr())),
      vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
  notification->set_priority(message_center::SYSTEM_PRIORITY);
  notification->set_pinned(pinned);
  notification->set_buttons(notification_actions);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void AdbSideloadingPolicyChangeNotification::HandleNotificationClick(
    std::optional<int> button_index) {
  // Only request restart when the button is clicked, i.e. ignore the clicks
  // on the body of the notification.
  if (!button_index)
    return;

  DCHECK(*button_index == 0);

  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER,
      "adb sideloading disable notification");
}
}  // namespace ash
