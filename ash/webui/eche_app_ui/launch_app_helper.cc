// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/launch_app_helper.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

namespace {

// TODO(b/265173006): Create an AppMetricsRecorder class to abstract metrics
// logging from implementation classes.
constexpr base::TimeDelta kPackageSetResetFrequency = base::Days(1);

}  // namespace

LaunchAppHelper::NotificationInfo::NotificationInfo(
    Category category,
    absl::variant<NotificationType, mojom::WebNotificationType> type)
    : category_(category), type_(type) {
  DCHECK(nullptr != absl::get_if<NotificationType>(&type)
             ? category == Category::kNative
             : category == Category::kWebUI);
  DCHECK(nullptr != absl::get_if<mojom::WebNotificationType>(&type)
             ? category == Category::kWebUI
             : category == Category::kNative);
}

LaunchAppHelper::NotificationInfo::~NotificationInfo() = default;

LaunchAppHelper::LaunchAppHelper(
    phonehub::PhoneHubManager* phone_hub_manager,
    LaunchEcheAppFunction launch_eche_app_function,
    LaunchNotificationFunction launch_notification_function,
    CloseNotificationFunction close_notification_function)
    : phone_hub_manager_(phone_hub_manager),
      launch_eche_app_function_(launch_eche_app_function),
      launch_notification_function_(launch_notification_function),
      close_notification_function_(close_notification_function) {}
LaunchAppHelper::~LaunchAppHelper() = default;

LaunchAppHelper::AppLaunchProhibitedReason
LaunchAppHelper::CheckAppLaunchProhibitedReason(FeatureStatus status) const {
  if (IsScreenLockRequired()) {
    return LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock;
  }

  return LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited;
}

bool LaunchAppHelper::IsScreenLockRequired() const {
  const bool enable_phone_screen_lock =
      phone_hub_manager_->GetScreenLockManager()->GetLockStatus() ==
      phonehub::ScreenLockManager::LockStatus::kLockedOn;
  auto* session_controller = ash::Shell::Get()->session_controller();
  const bool enable_cros_screen_lock =
      session_controller->CanLockScreen() &&
      session_controller->ShouldLockScreenAutomatically();
  // We should ask users to enable screen lock if they enabled screen lock on
  // their phone but didn't enable yet on CrOS.
  const bool should_lock = enable_phone_screen_lock && !enable_cros_screen_lock;
  return should_lock;
}

void LaunchAppHelper::ShowNotification(
    const std::optional<std::u16string>& title,
    const std::optional<std::u16string>& message,
    std::unique_ptr<NotificationInfo> info) const {
  launch_notification_function_.Run(title, message, std::move(info));
}

void LaunchAppHelper::CloseNotification(
    const std::string& notification_id) const {
  close_notification_function_.Run(notification_id);
}

void LaunchAppHelper::ShowToast(const std::u16string& text) const {
  ash::ToastManager::Get()->Show(ash::ToastData(
      kEcheAppToastId, ash::ToastCatalogName::kEcheAppToast, text));
}

void LaunchAppHelper::LaunchEcheApp(
    std::optional<int64_t> notification_id,
    const std::string& package_name,
    const std::u16string& visible_name,
    const std::optional<int64_t>& user_id,
    const gfx::Image& icon,
    const std::u16string& phone_name,
    AppsLaunchInfoProvider* apps_launch_info_provider) {
  launch_eche_app_function_.Run(notification_id, package_name, visible_name,
                                user_id, icon, phone_name,
                                apps_launch_info_provider);

  // Sessions can last for well over a day, so this check exists to cover that
  // corner case and clears the |session_packages_launched_| set so we can
  // start tracking unique packages again.
  // TODO(b/265172591): Optimize the reset to align with histogram uploads.
  if (session_packages_last_reset_ == base::TimeTicks() ||
      base::TimeTicks::Now() - session_packages_last_reset_ >=
          kPackageSetResetFrequency) {
    session_packages_launched_.clear();
    session_packages_last_reset_ = base::TimeTicks::Now();
  }

  if (!session_packages_launched_.contains(package_name)) {
    base::UmaHistogramCounts1000("Eche.UniqueAppsStreamed.PerDay", 1);
    session_packages_launched_.insert(package_name);
  }
}

}  // namespace eche_app
}  // namespace ash
