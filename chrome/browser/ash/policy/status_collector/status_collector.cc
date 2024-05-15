// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/status_collector.h"

#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/status_collector/activity_storage.h"
#include "chrome/browser/ash/policy/status_collector/app_info_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user_manager.h"

namespace policy {
namespace {

namespace ent_mgmt = ::enterprise_management;

// Returns the DeviceLocalAccount associated with the current kiosk session.
// Returns nullptr if there is no active kiosk session, or if that kiosk
// session has been removed from policy since the session started, in which
// case we won't report its status).
std::unique_ptr<DeviceLocalAccount> GetCurrentKioskDeviceLocalAccount(
    ash::CrosSettings* settings) {
  if (!user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return nullptr;
  }
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const std::vector<DeviceLocalAccount> accounts =
      GetDeviceLocalAccounts(settings);

  for (const auto& device_local_account : accounts) {
    if (AccountId::FromUserEmail(device_local_account.user_id) ==
        user->GetAccountId()) {
      return std::make_unique<DeviceLocalAccount>(device_local_account);
    }
  }
  LOG(WARNING) << "Kiosk app not found in list of device-local accounts";
  return nullptr;
}

}  // namespace

// -----------------------------------------------------------------------------
// StatusCollectorParams.
// -----------------------------------------------------------------------------

StatusCollectorParams::StatusCollectorParams() {
  device_status = std::make_unique<ent_mgmt::DeviceStatusReportRequest>();
  session_status = std::make_unique<ent_mgmt::SessionStatusReportRequest>();
  child_status = std::make_unique<ent_mgmt::ChildStatusReportRequest>();
}
StatusCollectorParams::~StatusCollectorParams() = default;

// Move only.
StatusCollectorParams::StatusCollectorParams(StatusCollectorParams&&) = default;
StatusCollectorParams& StatusCollectorParams::operator=(
    StatusCollectorParams&&) = default;

// -----------------------------------------------------------------------------
// StatusCollector.
// -----------------------------------------------------------------------------
// static
void StatusCollector::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kReportArcStatusEnabled, false);

  // TODO(crbug.com/40569404): move to ChildStatusCollector after migration.
  registry->RegisterDictionaryPref(prefs::kUserActivityTimes);
  registry->RegisterTimePref(prefs::kLastChildScreenTimeReset, base::Time());
  registry->RegisterTimePref(prefs::kLastChildScreenTimeSaved, base::Time());
  registry->RegisterIntegerPref(prefs::kChildScreenTimeMilliseconds, 0);

  AppInfoGenerator::RegisterProfilePrefs(registry);
}

// static
std::optional<std::string> StatusCollector::GetBootMode(
    ash::system::StatisticsProvider* statistics_provider) {
  const std::optional<std::string_view> dev_switch_mode =
      statistics_provider->GetMachineStatistic(ash::system::kDevSwitchBootKey);
  if (!dev_switch_mode) {
    return std::nullopt;
  }

  if (dev_switch_mode == ash::system::kDevSwitchBootValueDev) {
    return std::string("Dev");
  }

  if (dev_switch_mode == ash::system::kDevSwitchBootValueVerified) {
    return std::string("Verified");
  }

  return std::nullopt;
}

StatusCollector::StatusCollector(ash::system::StatisticsProvider* provider,
                                 ash::CrosSettings* cros_settings,
                                 base::Clock* clock)
    : statistics_provider_(provider),
      cros_settings_(cros_settings),
      clock_(clock) {}

StatusCollector::~StatusCollector() = default;

std::unique_ptr<DeviceLocalAccount>
StatusCollector::GetAutoLaunchedKioskSessionInfo() {
  std::unique_ptr<DeviceLocalAccount> account =
      GetCurrentKioskDeviceLocalAccount(ash::CrosSettings::Get());
  if (!account) {
    // No auto-launched kiosk session active.
    return nullptr;
  }

  ash::KioskChromeAppManager::App current_app;
  bool regular_app_auto_launched_with_zero_delay =
      ash::KioskChromeAppManager::Get()->GetApp(account->kiosk_app_id,
                                                &current_app) &&
      current_app.was_auto_launched_with_zero_delay;

  bool web_app_auto_launched_with_zero_delay =
      ash::WebKioskAppManager::Get()
          ->current_app_was_auto_launched_with_zero_delay();

  return regular_app_auto_launched_with_zero_delay ||
                 web_app_auto_launched_with_zero_delay
             ? std::move(account)
             : nullptr;
}

std::string StatusCollector::GetDMTokenForProfile(Profile* profile) const {
  CloudPolicyManager* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  DCHECK(user_cloud_policy_manager != nullptr);
  return user_cloud_policy_manager->core()->client()->dm_token();
}

}  // namespace policy
