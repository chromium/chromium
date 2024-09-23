// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_low_disk_notification.h"

#include <stdint.h>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

const char kLowDiskId[] = "crostini_low_disk";
const char kNotifierLowDisk[] = "crostini.disk";
const uint64_t kNotificationThreshold = 1 << 30;          // 1GB
const uint64_t kNotificationSevereThreshold = 512 << 20;  // 512MB
constexpr base::TimeDelta kNotificationInterval = base::Minutes(2);

ash::CiceroneClient* GetCiceroneClient() {
  return ash::CiceroneClient::Get();
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
  if (signal.vm_name() != kCrostiniDefaultVmName) {
    // TODO(crbug.com/40755190): Support VMs with different names
    return;
  }
  ShowNotificationIfAppropriate(signal.free_bytes());
}

void CrostiniLowDiskNotification::ShowNotificationIfAppropriate(
    uint64_t free_bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool show_low_disk_space_notification = true;
  Severity severity = GetSeverity(free_bytes);
  if (severity == Severity::NONE) {
    return;
  }
  if (!ash::CrosSettings::Get()->GetBoolean(
          ash::kDeviceShowLowDiskSpaceNotification,
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
  base::Time now = base::Time::Now();
  if (severity != last_notification_severity_ ||
      (severity == Severity::HIGH &&
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
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierLowDisk,
      ash::NotificationCatalogName::kCrostiniLowDisk);

  auto on_click = base::BindRepeating([](std::optional<int> button_index) {
    if (button_index) {
      DCHECK_EQ(0, *button_index);
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(),
          chromeos::settings::mojom::kCrostiniDetailsSubpagePath);
    }
  });
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
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
