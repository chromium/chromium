// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/status_collector/activity_storage.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
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
    chromeos::CrosSettings* settings) {
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

  // TODO(crbug.com/827386): move to ChildStatusCollector after migration.
  registry->RegisterDictionaryPref(prefs::kUserActivityTimes);
  registry->RegisterTimePref(prefs::kLastChildScreenTimeReset, base::Time());
  registry->RegisterTimePref(prefs::kLastChildScreenTimeSaved, base::Time());
  registry->RegisterIntegerPref(prefs::kChildScreenTimeMilliseconds, 0);
}

// static
base::Optional<std::string> StatusCollector::GetBootMode(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string dev_switch_mode;
  if (!statistics_provider->GetMachineStatistic(
          chromeos::system::kDevSwitchBootKey, &dev_switch_mode)) {
    return base::nullopt;
  }

  if (dev_switch_mode == chromeos::system::kDevSwitchBootValueDev) {
    return std::string("Dev");
  }

  if (dev_switch_mode == chromeos::system::kDevSwitchBootValueVerified) {
    return std::string("Verified");
  }

  return base::nullopt;
}

StatusCollector::StatusCollector(chromeos::system::StatisticsProvider* provider,
                                 chromeos::CrosSettings* cros_settings)
    : statistics_provider_(provider), cros_settings_(cros_settings) {}

StatusCollector::~StatusCollector() = default;

std::unique_ptr<DeviceLocalAccount>
StatusCollector::GetAutoLaunchedKioskSessionInfo() {
  std::unique_ptr<DeviceLocalAccount> account =
      GetCurrentKioskDeviceLocalAccount(chromeos::CrosSettings::Get());
  if (!account) {
    // No auto-launched kiosk session active.
    return nullptr;
  }

  chromeos::KioskAppManager::App current_app;
  bool regular_app_auto_launched_with_zero_delay =
      chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                               &current_app) &&
      current_app.was_auto_launched_with_zero_delay;
  bool arc_app_auto_launched_with_zero_delay =
      chromeos::ArcKioskAppManager::Get()
          ->current_app_was_auto_launched_with_zero_delay();

  bool web_app_auto_launched_with_zero_delay =
      chromeos::WebKioskAppManager::Get()
          ->current_app_was_auto_launched_with_zero_delay();

  return regular_app_auto_launched_with_zero_delay ||
                 arc_app_auto_launched_with_zero_delay ||
                 web_app_auto_launched_with_zero_delay
             ? std::move(account)
             : nullptr;
}

std::string StatusCollector::GetDMTokenForProfile(Profile* profile) const {
  CloudPolicyManager* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  DCHECK(user_cloud_policy_manager != nullptr);
  return user_cloud_policy_manager->core()->client()->dm_token();
}

base::Time StatusCollector::GetCurrentTime() {
  return base::Time::Now();
}

}  // namespace policy
