// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_update_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
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
  registry->RegisterDictionaryPref(kIwaKioskDictionaryName);
}

// static
KioskIwaManager* KioskIwaManager::Get() {
  CHECK(g_kiosk_iwa_manager_instance);
  return g_kiosk_iwa_manager_instance;
}

KioskIwaManager::KioskIwaManager(PrefService& local_state,
                                 KioskCryptohomeRemover* cryptohome_remover)
    : KioskAppManagerBase(&local_state, cryptohome_remover) {
  CHECK(!g_kiosk_iwa_manager_instance);  // Only one instance is allowed.
  g_kiosk_iwa_manager_instance = this;
  UpdateAppsFromPolicy();
}

KioskIwaManager::~KioskIwaManager() {
  g_kiosk_iwa_manager_instance = nullptr;
}

KioskAppManagerBase::AppList KioskIwaManager::GetApps() const {
  AppList result;
  for (const auto& iwa_app_data : isolated_web_apps_) {
    result.emplace_back(*iwa_app_data);
  }
  return result;
}

const KioskIwaData* KioskIwaManager::GetApp(const AccountId& account_id) const {
  const auto iter = std::ranges::find_if(
      isolated_web_apps_,
      [&account_id](const std::unique_ptr<KioskIwaData>& app) {
        return app->account_id() == account_id;
      });

  if (iter == isolated_web_apps_.end()) {
    return nullptr;
  }
  return iter->get();
}

void KioskIwaManager::UpdateApp(const AccountId& account_id,
                                const std::string& title,
                                const GURL& /*start_url*/,
                                const web_app::IconBitmaps& icon_bitmaps) {
  for (auto& iwa_data : isolated_web_apps_) {
    if (iwa_data->account_id() == account_id) {
      iwa_data->Update(title, icon_bitmaps);
      return;
    }
  }
  NOTREACHED();
}

const std::optional<AccountId>& KioskIwaManager::GetAutoLaunchAccountId()
    const {
  return auto_launch_id_;
}

void KioskIwaManager::OnKioskSessionStarted(const KioskAppId& app_id) {
  CHECK_EQ(app_id.type, KioskAppType::kIsolatedWebApp);
  NotifySessionInitialized();
}

void KioskIwaManager::StartObservingAppUpdate(Profile* profile,
                                              const AccountId& account_id) {
  app_update_observer_ = std::make_unique<chromeos::KioskWebAppUpdateObserver>(
      profile, account_id, KioskIwaData::kIconSize,
      base::BindRepeating(&KioskIwaManager::UpdateApp,
                          weak_ptr_factory_.GetWeakPtr()));
}

void KioskIwaManager::AddAppForTesting(
    const policy::DeviceLocalAccount& account) {
  KioskIwaDataMap dummy;
  ProcessDeviceLocalAccount(account, dummy);
  NotifyKioskAppsChanged();
}

void KioskIwaManager::UpdateAppsFromPolicy() {
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
    cryptohome_remover_->CancelDelayedCryptohomeRemoval(iwa->account_id());
  }
}

void KioskIwaManager::RemoveApps(const KioskIwaDataMap& previous_apps) const {
  std::vector<const KioskAppDataBase*> apps_to_remove;
  std::ranges::transform(previous_apps, std::back_inserter(apps_to_remove),
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

  auto new_iwa_data = KioskIwaData::Create(
      account.user_id, account.kiosk_iwa_info, *this, local_state_.get());

  if (!new_iwa_data) {
    LOG(WARNING) << "Cannot create Kiosk IWA data for account "
                 << account.account_id;
    return;
  }

  // Check the new app entry against existing apps.
  auto previous_match = previous_apps.find(new_iwa_data->app_id());
  if (previous_match != previous_apps.end()) {
    // Keep this app from deletion.
    auto previous_iwa_data = std::move(previous_match->second);
    previous_apps.erase(previous_match);

    // Replace with the new IWA entry if there are changes (e.g. different
    // update manifest URL or version setting).
    if (*new_iwa_data != *previous_iwa_data) {
      isolated_web_apps_.push_back(std::move(new_iwa_data));
      isolated_web_apps_.back()->LoadFromCache();
    } else {
      isolated_web_apps_.push_back(std::move(previous_iwa_data));
    }
  } else {
    // Add the new IWA entry (no previous matches).
    isolated_web_apps_.push_back(std::move(new_iwa_data));
    isolated_web_apps_.back()->LoadFromCache();
  }

  MaybeSetAutoLaunchInfo(account.account_id,
                         isolated_web_apps_.back()->account_id());
}

}  // namespace ash
