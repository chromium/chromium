// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "url/origin.h"

namespace ash {

namespace {
KioskIwaManager* g_kiosk_iwa_manager_instance = nullptr;

// TODO(crbug.com/375623747): Make common helpers for all kiosk app managers.
std::string GetAutoLoginIdSetting() {
  std::string auto_login_id_setting;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_id_setting);
  return auto_login_id_setting;
}

int GetAutoLoginDelaySetting() {
  int auto_login_delay_setting = 0;

  // unset setting also defaults to zero.
  CrosSettings::Get()->GetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay,
                                  &auto_login_delay_setting);
  return auto_login_delay_setting;
}
}  // namespace

// static
const char KioskIwaManager::kIwaKioskDictionaryName[] = "iwa-kiosk";

// static
void KioskIwaManager::RegisterPrefs(PrefRegistrySimple* registry) {
  if (!ash::features::IsIsolatedWebAppKioskEnabled()) {
    return;
  }
  registry->RegisterDictionaryPref(kIwaKioskDictionaryName);
}

// static
KioskIwaManager* KioskIwaManager::Get() {
  CHECK(g_kiosk_iwa_manager_instance);
  return g_kiosk_iwa_manager_instance;
}

KioskIwaManager::KioskIwaManager() {
  CHECK(!g_kiosk_iwa_manager_instance);  // Only one instance is allowed.
  g_kiosk_iwa_manager_instance = this;
  UpdateAppsFromPolicy();
}

KioskIwaManager::~KioskIwaManager() {
  g_kiosk_iwa_manager_instance = nullptr;
}

KioskAppManagerBase::AppList KioskIwaManager::GetApps() const {
  if (!ash::features::IsIsolatedWebAppKioskEnabled()) {
    return {};
  }

  AppList result;
  for (const auto& iwa_app_data : isolated_web_apps_) {
    // TODO(crbug.com/361017701): fill in the install url
    result.emplace_back(*iwa_app_data);
  }
  return result;
}

const KioskIwaData* KioskIwaManager::GetApp(const AccountId& account_id) const {
  if (!ash::features::IsIsolatedWebAppKioskEnabled()) {
    return nullptr;
  }

  const auto iter = base::ranges::find_if(
      isolated_web_apps_,
      [&account_id](const std::unique_ptr<KioskIwaData>& app) {
        return app->account_id() == account_id;
      });

  if (iter == isolated_web_apps_.end()) {
    return nullptr;
  }
  return iter->get();
}

const std::optional<AccountId>& KioskIwaManager::GetAutoLaunchAccountId()
    const {
  return auto_launch_id_;
}

void KioskIwaManager::OnKioskSessionStarted(const KioskAppId& app_id) {
  CHECK_EQ(app_id.type, KioskAppType::kIsolatedWebApp);
  NotifySessionInitialized();
}

void KioskIwaManager::UpdateAppsFromPolicy() {
  if (!ash::features::IsIsolatedWebAppKioskEnabled()) {
    // keeps KioskIwaManager empty if the feature is disabled.
    Reset();
    return;
  }

  auto previous_apps = GetAppsAndReset();

  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (const policy::DeviceLocalAccount& account : device_local_accounts) {
    ProcessDeviceLocalAccount(account, previous_apps);
  }

  CancelCryptohomeRemovalsForCurrentApps();
  RemoveApps(previous_apps);
  NotifyKioskAppsChanged();
}

void KioskIwaManager::Reset() {
  isolated_web_apps_.clear();
  auto_launch_id_.reset();
  auto_launched_with_zero_delay_ = false;
}

void KioskIwaManager::MaybeSetAutoLaunchInfo(
    const std::string& policy_account_id,
    const AccountId& kiosk_app_account_id) {
  const auto auto_login_id_setting = GetAutoLoginIdSetting();

  if (!auto_login_id_setting.empty() &&
      auto_login_id_setting == policy_account_id) {
    auto_launch_id_ = kiosk_app_account_id;
    // TODO(crbug.com/375620498): remove auto_launched_with_zero_delay_.
    // Kiosk mode only supports immediate auto launch.
    auto_launched_with_zero_delay_ = (GetAutoLoginDelaySetting() == 0);
  }
}

void KioskIwaManager::CancelCryptohomeRemovalsForCurrentApps() {
  for (const auto& iwa : isolated_web_apps_) {
    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(iwa->account_id());
  }
}

void KioskIwaManager::RemoveApps(const KioskIwaDataMap& previous_apps) const {
  std::vector<KioskAppDataBase*> apps_to_remove;
  base::ranges::transform(previous_apps, std::back_inserter(apps_to_remove),
                          [](const auto& kv) { return kv.second.get(); });
  ClearRemovedApps(apps_to_remove);
}

KioskIwaManager::KioskIwaDataMap KioskIwaManager::GetAppsAndReset() {
  KioskIwaDataMap result;
  for (auto& app : isolated_web_apps_) {
    result[app->app_id()] = std::move(app);
  }
  Reset();
  return result;
}

void KioskIwaManager::ProcessDeviceLocalAccount(
    const policy::DeviceLocalAccount& account,
    KioskIwaDataMap& previous_apps) {
  if (account.type != policy::DeviceLocalAccountType::kKioskIsolatedWebApp) {
    return;
  }
  const std::string& web_bundle_id = account.kiosk_iwa_info.web_bundle_id();
  const GURL update_manifest_url(account.kiosk_iwa_info.update_manifest_url());

  auto new_iwa_data =
      KioskIwaData::Create(account.user_id, web_bundle_id, update_manifest_url);

  if (!new_iwa_data) {
    LOG(WARNING) << "Cannot create Kiosk IWA data for account "
                 << account.account_id;
    return;
  }

  // TODO(crbug.com/378065964): Revisit app data processing below after
  // implementing icon and title.
  auto previous_match = previous_apps.find(new_iwa_data->app_id());
  if (previous_match != previous_apps.end()) {
    // Reuse the already existing app data and keep this app from deletion.
    auto previous_iwa_data = std::move(previous_match->second);
    previous_apps.erase(previous_match);

    // But still replace with a new IWA entry if manifest URL changed.
    if (new_iwa_data->update_manifest_url() !=
        previous_iwa_data->update_manifest_url()) {
      isolated_web_apps_.push_back(std::move(new_iwa_data));
    } else {
      isolated_web_apps_.push_back(std::move(previous_iwa_data));
    }
  } else {
    // Add a new IWA entry (no existing matches).
    isolated_web_apps_.push_back(std::move(new_iwa_data));
  }

  MaybeSetAutoLaunchInfo(account.account_id,
                         isolated_web_apps_.back()->account_id());
}

}  // namespace ash
