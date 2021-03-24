// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_low_disk_notification.h"

#include <stdint.h>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

const char kLowDiskId[] = "crostini_low_disk";
const char kNotifierLowDisk[] = "crostini.disk";
const uint64_t kNotificationThreshold = 1 << 30;          // 1GB
const uint64_t kNotificationSevereThreshold = 512 << 20;  // 512MB
constexpr base::TimeDelta kNotificationInterval =
    base::TimeDelta::FromMinutes(2);

chromeos::CiceroneClient* GetCiceroneClient() {
  return chromeos::DBusThreadManager::Get()->GetCiceroneClient();
}

}  // namespace

namespace crostini {

CrostiniLowDiskNotification::CrostiniLowDiskNotification()
    : notification_interval_(kNotificationInterval) {
  GetCiceroneClient()->AddObserver(this);
}

CrostiniLowDiskNotification::~CrostiniLowDiskNotification() {
  GetCiceroneClient()->RemoveObserver(this);
}

void CrostiniLowDiskNotification::OnLowDiskSpaceTriggered(
    const vm_tools::cicerone::LowDiskSpaceTriggeredSignal& signal) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (signal.vm_name() != kCrostiniDefaultVmName) {
    // TODO(crbug/1189009): Support VMs with different names
    return;
  }

  bool show_low_disk_space_notification = true;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kDeviceShowLowDiskSpaceNotification,
          &show_low_disk_space_notification)) {
    DVLOG(1) << "DeviceShowLowDiskSpaceNotification not set, "
                "defaulting to showing the notification.";
  }

  // We suppress the low-space notifications when there are multiple users on an
  // enterprise managed device based on policy configuration.
  if (!show_low_disk_space_notification &&
      user_manager::UserManager::Get()->GetUsers().size() > 1) {
    LOG(WARNING) << "Crostini is low on disk space, but the notification was "
                 << "suppressed on a managed device.";
    return;
  }
  Severity severity = GetSeverity(signal.free_bytes());
  base::Time now = base::Time::Now();
  if (severity != last_notification_severity_ ||
      (severity == HIGH &&
       now - last_notification_time_ > notification_interval_)) {
    SystemNotificationHelper::GetInstance()->Display(
        *CreateNotification(severity));
    last_notification_time_ = now;
    last_notification_severity_ = severity;
  }
}

std::unique_ptr<message_center::Notification>
CrostiniLowDiskNotification::CreateNotification(Severity severity) {
  std::u16string title;
  std::u16string message;
  message_center::SystemNotificationWarningLevel warning_level;
  if (severity == Severity::HIGH) {
    title = l10n_util::GetStringUTF16(
        IDS_CROSTINI_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
    message = l10n_util::GetStringUTF16(
        IDS_CROSTINI_CRITICALLY_LOW_DISK_NOTIFICATION_MESSAGE);
    warning_level =
        message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else {
    title = l10n_util::GetStringUTF16(IDS_CROSTINI_LOW_DISK_NOTIFICATION_TITLE);
    message =
        l10n_util::GetStringUTF16(IDS_CROSTINI_LOW_DISK_NOTIFICATION_MESSAGE);
    warning_level = message_center::SystemNotificationWarningLevel::WARNING;
  }

  message_center::ButtonInfo storage_settings(
      l10n_util::GetStringUTF16(IDS_CROSTINI_LOW_DISK_NOTIFICATION_BUTTON));
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(storage_settings);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierLowDisk);

  auto on_click = base::BindRepeating([](base::Optional<int> button_index) {
    if (button_index) {
      DCHECK_EQ(0, *button_index);
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(),
          chromeos::settings::mojom::kCrostiniDetailsSubpagePath);
    }
  });
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kLowDiskId, title, message,
          std::u16string(), GURL(), notifier_id, optional_fields,
          new message_center::HandleNotificationClickDelegate(on_click),
          kNotificationStorageFullIcon, warning_level);

  return notification;
}

CrostiniLowDiskNotification::Severity CrostiniLowDiskNotification::GetSeverity(
    uint64_t free_disk_bytes) {
  if (free_disk_bytes < kNotificationSevereThreshold) {
    return Severity::HIGH;
  }
  if (free_disk_bytes < kNotificationThreshold) {
    return Severity::MEDIUM;
  }
  return Severity::NONE;
}

void CrostiniLowDiskNotification::SetNotificationIntervalForTest(
    base::TimeDelta notification_interval) {
  notification_interval_ = notification_interval;
}

}  // namespace crostini
