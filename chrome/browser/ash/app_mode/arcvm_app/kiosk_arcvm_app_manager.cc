// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_manager.h"

#include <algorithm>
#include <map>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// This class is owned by `ChromeBrowserMainPartsAsh`.
KioskArcvmAppManager* g_arcvm_kiosk_app_manager = nullptr;

}  // namespace

// static
void KioskArcvmAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  // TODO(crbug.com/418941003): Move dictionary name constant to this class or
  // move this method to KioskArcvmAppData.
  registry->RegisterDictionaryPref(
      KioskArcvmAppData::kArcvmKioskDictionaryName);
}

// static
KioskArcvmAppManager* KioskArcvmAppManager::Get() {
  return g_arcvm_kiosk_app_manager;
}

KioskArcvmAppManager::KioskArcvmAppManager(PrefService* local_state)
    : auto_launch_account_id_(EmptyAccountId()), local_state_(local_state) {
  CHECK(!g_arcvm_kiosk_app_manager);  // Only one instance is allowed.
  g_arcvm_kiosk_app_manager = this;
  UpdateAppsFromPolicy();
}

KioskArcvmAppManager::~KioskArcvmAppManager() {
  CHECK_EQ(g_arcvm_kiosk_app_manager, this);
  g_arcvm_kiosk_app_manager = nullptr;
}

const KioskArcvmAppData* KioskArcvmAppManager::GetAppByAccountId(
    const AccountId& account_id) const {
  for (auto& app : apps_) {
    if (app->account_id() == account_id) {
      return app.get();
    }
  }
  return nullptr;
}

std::vector<KioskArcvmAppManager::App> KioskArcvmAppManager::GetApps() const {
  std::vector<App> apps;
  for (const auto& app : apps_) {
    apps.emplace_back(*app);
  }
  return apps;
}

std::vector<const KioskArcvmAppData*> KioskArcvmAppManager::GetAppsForTesting()
    const {
  std::vector<const KioskArcvmAppData*> apps;
  for (const auto& app : apps_) {
    apps.push_back(app.get());
  }
  return apps;
}

void KioskArcvmAppManager::UpdateNameAndIcon(const AccountId& account_id,
                                             const std::string& name,
                                             const gfx::ImageSkia& icon) {
  for (auto& app : apps_) {
    if (app->account_id() == account_id) {
      app->SetCache(name, icon, GetKioskAppIconCacheDir());
      return;
    }
  }
  LOG(ERROR) << "Could not an find app with matching acconut ID, name and icon "
                "not updated";
}

void KioskArcvmAppManager::AddAutoLaunchAppForTest(
    const std::string& app_id,
    const policy::ArcvmKioskAppBasicInfo& app_info,
    const AccountId& account_id) {
  for (auto it = apps_.begin(); it != apps_.end(); ++it) {
    if ((*it)->app_id() == app_id) {
      apps_.erase(it);
      break;
    }
  }

  apps_.emplace_back(std::make_unique<KioskArcvmAppData>(
      local_state_, app_id, app_info.package_name(), app_info.class_name(),
      app_info.action(), account_id, app_info.display_name()));

  auto_launch_account_id_ = account_id;
  auto_launched_with_zero_delay_ = true;
  NotifyKioskAppsChanged();
}

const AccountId& KioskArcvmAppManager::GetAutoLaunchAccountId() const {
  return auto_launch_account_id_;
}

void KioskArcvmAppManager::UpdateAppsFromPolicy() {
  // Do not populate ARC kiosk apps if ARC kiosk apps can't be run on the
  // device.
  // Apps won't be added to kiosk Apps menu and won't be auto-launched.
  if (!arc::IsArcAvailable() || !ash::features::IsHeliumArcvmKioskEnabled()) {
    VLOG(1) << "Device doesn't support ARC kiosk";
    return;
  }

  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  std::map<std::string, std::unique_ptr<KioskArcvmAppData>> old_apps;
  for (auto& app : apps_) {
    old_apps.try_emplace(app->app_id(), std::move(app));
  }
  apps_.clear();
  auto_launch_account_id_.clear();
  auto_launched_with_zero_delay_ = false;
  std::string auto_login_account_id_from_settings;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id_from_settings);

  // Re-populates `apps_` and reuses existing apps when possible.
  // TODO(crbug.com/418846233): Check if account info can be fetched from
  // the UserManager.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (const auto& account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccountType::kArcvmKioskApp) {
      continue;
    }

    const AccountId account_id(AccountId::FromUserEmail(account.user_id));

    // This will set the auto launch account ID to the last account ID that
    // matches the auto login ID from settings.
    // TODO(crbug.com/418941755): Add CHECK to see if multiple apps are
    // qualified to be launched.
    if (account.account_id == auto_login_account_id_from_settings) {
      auto_launch_account_id_ = account_id;
      auto_launched_with_zero_delay_ = true;
    }

    const policy::ArcvmKioskAppBasicInfo& app_info =
        account.arcvm_kiosk_app_info;
    std::string app_id = ArcAppListPrefs::GetAppId(app_info.package_name(),
                                                   app_info.class_name().empty()
                                                       ? app_info.action()
                                                       : app_info.class_name());

    auto old_it = old_apps.find(app_id);
    if (old_it != old_apps.end()) {
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      // Use package name when display name is not specified.
      std::string name = app_info.display_name().empty()
                             ? app_info.package_name()
                             : app_info.display_name();
      apps_.push_back(std::make_unique<KioskArcvmAppData>(
          local_state_, app_id, app_info.package_name(), app_info.class_name(),
          app_info.action(), account_id, name));
      apps_.back()->LoadFromCache();
    }
    // TODO(crbug.com/418847377): Remove direct dependency on
    // KioskCryptohomeRemover.
    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(account_id);
  }

  std::vector<const KioskAppDataBase*> old_apps_to_remove;
  for (auto& entry : old_apps) {
    old_apps_to_remove.emplace_back(entry.second.get());
  }
  ClearRemovedApps(old_apps_to_remove);

  NotifyKioskAppsChanged();
}

}  // namespace ash
