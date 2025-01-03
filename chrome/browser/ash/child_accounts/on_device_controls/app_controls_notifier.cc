// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_notifier.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/user_metrics.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_service_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace {
constexpr char kShowNotificationId[] = "show_app_controls_notification";

constexpr int kAppControlsMinimumVersion = 127;

// Action names should be kept in sync with corresponding actions in
// src/tools/metrics/actions/actions.xml.
constexpr char kNotificationClickedActionName[] =
    "OnDeviceControls_NotificationClicked";

constexpr char kNotificationShownActionName[] =
    "OnDeviceControls_NotificationShown";
}  // namespace

namespace ash::on_device_controls {

// static
void AppControlsNotifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOnDeviceAppControlsNotificationShown,
                                false);
}

AppControlsNotifier::AppControlsNotifier(Profile* profile)
    : profile_(profile) {}

AppControlsNotifier::~AppControlsNotifier() = default;

void AppControlsNotifier::MaybeShowAppControlsNotification() {
  if (!ShouldShowNotification()) {
    return;
  }
  ShowNotification();
}

void AppControlsNotifier::HandleClick(std::optional<int> button_index) {
  profile_->GetPrefs()->SetBoolean(prefs::kOnDeviceAppControlsNotificationShown,
                                   true);
  if (!button_index) {
    return;
  }
  base::RecordAction(base::UserMetricsAction(kNotificationClickedActionName));
  OpenAppsSettings();
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kShowNotificationId);
}

void AppControlsNotifier::OpenAppsSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kAppsSectionPath);
}

bool AppControlsNotifier::ShouldShowNotification() const {
  if (!AppControlsServiceFactory::IsOnDeviceAppControlsAvailable(profile_)) {
    return false;
  }

  // Skip notifying if the notification has been shown to the user before.
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kOnDeviceAppControlsNotificationShown)) {
    return false;
  }

  return version_info::GetMajorVersionNumberAsInt() >=
         kAppControlsMinimumVersion;
}

void AppControlsNotifier::ShowNotification() {
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_ON_DEVICE_APP_CONTROLS_NOTIFICATION_TITLE);
  std::u16string message = ui::SubstituteChromeOSDeviceType(
      IDS_ON_DEVICE_APP_CONTROLS_NOTIFICATION_MESSAGE);
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ON_DEVICE_APP_CONTROLS_NOTIFICATION_OPEN_SETTINGS_BUTTON_LABEL));

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kShowNotificationId, title,
      message, /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kShowNotificationId,
                                 NotificationCatalogName::kOnDeviceAppControls),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&AppControlsNotifier::HandleClick,
                              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
      message_center::SystemNotificationWarningLevel::NORMAL);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);

  base::RecordAction(base::UserMetricsAction(kNotificationShownActionName));
}

}  // namespace ash::on_device_controls
