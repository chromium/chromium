// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/low_disk_notification.h"

#include <stdint.h>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

const char kLowDiskId[] = "low_disk";
const char kNotifierLowDisk[] = "ash.disk";
const uint64_t kNotificationThreshold = 1 << 30;          // 1GB
const uint64_t kNotificationSevereThreshold = 512 << 20;  // 512MB
constexpr base::TimeDelta kNotificationInterval =
    base::TimeDelta::FromMinutes(2);

}  // namespace

namespace chromeos {

LowDiskNotification::LowDiskNotification()
    : notification_interval_(kNotificationInterval) {
  DCHECK(CryptohomeClient::Get());
  CryptohomeClient::Get()->AddObserver(this);
}

LowDiskNotification::~LowDiskNotification() {
  DCHECK(CryptohomeClient::Get());
  CryptohomeClient::Get()->RemoveObserver(this);
}

void LowDiskNotification::LowDiskSpace(uint64_t free_disk_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We suppress the low-space notifications when there are multiple users on an
  // enterprise managed device. crbug.com/656788.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged() &&
      user_manager::UserManager::Get()->GetUsers().size() > 1) {
    LOG(WARNING) << "Device is low on disk space, but the notification was "
                 << "suppressed on a managed device.";
    return;
  }
  Severity severity = GetSeverity(free_disk_bytes);
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
LowDiskNotification::CreateNotification(Severity severity) {
  base::string16 title;
  base::string16 message;
  message_center::SystemNotificationWarningLevel warning_level;
  if (severity == Severity::HIGH) {
    title =
        l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
    message =
        l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_MESSAGE);
    warning_level =
        message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else {
    title = l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_TITLE);
    message = l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_MESSAGE);
    warning_level = message_center::SystemNotificationWarningLevel::WARNING;
  }

  message_center::ButtonInfo storage_settings(
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_BUTTON));
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(storage_settings);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierLowDisk);

  auto on_click = base::BindRepeating([](base::Optional<int> button_index) {
    if (button_index) {
      DCHECK_EQ(0, *button_index);
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(), chrome::kStorageSubPage);
    }
  });
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kLowDiskId, title, message,
          base::string16(), GURL(), notifier_id, optional_fields,
          new message_center::HandleNotificationClickDelegate(on_click),
          kNotificationStorageFullIcon, warning_level);

  return notification;
}

LowDiskNotification::Severity LowDiskNotification::GetSeverity(
    uint64_t free_disk_bytes) {
  if (free_disk_bytes < kNotificationSevereThreshold)
    return Severity::HIGH;
  if (free_disk_bytes < kNotificationThreshold)
    return Severity::MEDIUM;
  return Severity::NONE;
}

void LowDiskNotification::SetNotificationIntervalForTest(
    base::TimeDelta notification_interval) {
  notification_interval_ = notification_interval;
}

}  // namespace chromeos
