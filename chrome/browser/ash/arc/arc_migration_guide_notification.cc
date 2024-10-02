// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_migration_guide_notification.h"

#include <memory>
#include <optional>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/arc_migration_constants.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "arc_fs_migration";
constexpr char kSuggestNotificationId[] = "arc_fs_migration/suggest";

}  // namespace

// static
void ShowArcMigrationGuideNotification(Profile* profile) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      ash::NotificationCatalogName::kArcMigrationGuide);
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();

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
          base::BindRepeating(&chrome::AttemptUserExit));

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kSuggestNotificationId,
      l10n_util::GetStringUTF16(IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_TITLE),
      message, std::u16string(), GURL(), notifier_id,
      message_center::RichNotificationData(), std::move(delegate),
      vector_icons::kSettingsIcon,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification.set_renotify(true);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

}  // namespace arc
