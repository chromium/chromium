// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_migration_guide_notification.h"

#include <memory>
#include <optional>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/arc_migration_constants.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "arc_fs_migration";

}  // namespace

// static
void ShowArcMigrationGuideNotification(const user_manager::User& user) {
  const std::string notification_id = ash::CreateUserScopedNotificationId(
      kSuggestNotificationId, user.username_hash());
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      ash::NotificationCatalogName::kArcMigrationGuide);
  notifier_id.profile_id = user.GetAccountId().GetUserEmail();

  std::optional<power_manager::PowerSupplyProperties> power =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  const bool is_low_battery =
      power &&
      power->battery_state() !=
          power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT &&
      power->battery_percent() < kMigrationMinimumBatteryPercent;

  const std::u16string message = ui::SubstituteChromeOSDeviceType(
      is_low_battery
          ? IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_LOW_BATTERY_MESSAGE
          : IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_MESSAGE);

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([]() {
            session_manager::SessionManager::Get()->RequestSignOut();
          }));

  auto notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_TITLE),
      message, std::u16string(), GURL(), notifier_id,
      message_center::RichNotificationData(), std::move(delegate),
      features::IsRoundedIconsEnabled() ? vector_icons::kSettingsFilledIcon
                                        : vector_icons::kSettingsOldIcon,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification->set_renotify(true);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace arc
