// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"

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
  // TODO(crbug.com/361017701): remove old apps as other app managers do.
  Clear();

  if (!ash::features::IsIsolatedWebAppKioskEnabled()) {
    // keeps KioskIwaManager empty if the feature is disabled.
    return;
  }

  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());

  for (const policy::DeviceLocalAccount& account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccountType::kKioskIsolatedWebApp) {
      continue;
    }

    const std::string web_bundle_id(account.kiosk_iwa_info.web_bundle_id());
    const GURL update_manifest_url(
        account.kiosk_iwa_info.update_manifest_url());

    auto iwa_data = KioskIwaData::Create(account.user_id, web_bundle_id,
                                         update_manifest_url);

    if (!iwa_data) {
      LOG(WARNING) << "Cannot add Kiosk IWA data for account "
                   << account.account_id;
      continue;
    }

    MaybeSetAutoLaunchInfo(account.account_id, iwa_data->account_id());

    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(
        iwa_data->account_id());
    isolated_web_apps_.push_back(std::move(iwa_data));
  }

  NotifyKioskAppsChanged();
}

void KioskIwaManager::Clear() {
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

}  // namespace ash
