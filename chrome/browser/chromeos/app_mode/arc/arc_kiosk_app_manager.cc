// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/chromeos/app_mode/pref_names.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

namespace {

// This class is owned by ChromeBrowserMainPartsChromeos.
static ArcKioskAppManager* g_arc_kiosk_app_manager = nullptr;

}  // namespace

// static
const char ArcKioskAppManager::kArcKioskDictionaryName[] = "arc-kiosk";

// static
void ArcKioskAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kArcKioskDictionaryName);
}

// static
ArcKioskAppManager* ArcKioskAppManager::Get() {
  return g_arc_kiosk_app_manager;
}

ArcKioskAppManager::ArcKioskAppManager()
    : auto_launch_account_id_(EmptyAccountId()) {
  DCHECK(!g_arc_kiosk_app_manager);  // Only one instance is allowed.
  g_arc_kiosk_app_manager = this;
  UpdateAppsFromPolicy();
}

ArcKioskAppManager::~ArcKioskAppManager() {
  g_arc_kiosk_app_manager = nullptr;
}

const ArcKioskAppData* ArcKioskAppManager::GetAppByAccountId(
    const AccountId& account_id) {
  for (auto& app : apps_) {
    if (app->account_id() == account_id)
      return app.get();
  }
  return nullptr;
}

void ArcKioskAppManager::GetApps(std::vector<App>* apps) const {
  apps->clear();
  apps->reserve(apps_.size());
  for (auto& app : apps_) {
    apps->emplace_back(*app.get());
  }
}

void ArcKioskAppManager::GetAppsForTesting(
    std::vector<const ArcKioskAppData*>* apps) const {
  apps->clear();
  apps->reserve(apps_.size());
  for (auto& app : apps_) {
    apps->push_back(app.get());
  }
}

void ArcKioskAppManager::UpdateNameAndIcon(const std::string& app_id,
                                           const std::string& name,
                                           const gfx::ImageSkia& icon) {
  for (auto& app : apps_) {
    if (app->app_id() == app_id) {
      app->SetCache(name, icon);
      return;
    }
  }
}

void ArcKioskAppManager::AddAutoLaunchAppForTest(
    const std::string& app_id,
    const policy::ArcKioskAppBasicInfo& app_info,
    const AccountId& account_id) {
  for (auto it = apps_.begin(); it != apps_.end(); ++it) {
    if ((*it)->app_id() == app_id) {
      apps_.erase(it);
      break;
    }
  }

  apps_.emplace_back(std::make_unique<ArcKioskAppData>(
      app_id, app_info.package_name(), app_info.class_name(), app_info.action(),
      account_id, app_info.display_name()));

  auto_launch_account_id_ = account_id;
  auto_launched_with_zero_delay_ = true;
}

const AccountId& ArcKioskAppManager::GetAutoLaunchAccountId() const {
  return auto_launch_account_id_;
}

void ArcKioskAppManager::UpdateAppsFromPolicy() {
  // Do not populate ARC kiosk apps if ARC kiosk apps can't be run on the
  // device.
  // Apps won't be added to kiosk Apps menu and won't be auto-launched.
  if (!arc::IsArcKioskAvailable()) {
    VLOG(1) << "Device doesn't support ARC kiosk";
    return;
  }

  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  std::map<std::string, std::unique_ptr<ArcKioskAppData>> old_apps;
  for (auto& app : apps_)
    old_apps[app->app_id()] = std::move(app);
  apps_.clear();
  auto_launch_account_id_.clear();
  auto_launched_with_zero_delay_ = false;
  std::string auto_login_account_id_from_settings;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id_from_settings);

  // Re-populates |apps_| and reuses existing apps when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (auto account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP)
      continue;

    const AccountId account_id(AccountId::FromUserEmail(account.user_id));

    if (account.account_id == auto_login_account_id_from_settings) {
      auto_launch_account_id_ = account_id;
      int auto_launch_delay = 0;
      CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &auto_launch_delay);
      auto_launched_with_zero_delay_ = auto_launch_delay == 0;
    }

    const policy::ArcKioskAppBasicInfo& app_info = account.arc_kiosk_app_info;
    std::string app_id;
    if (!app_info.class_name().empty()) {
      app_id = ArcAppListPrefs::GetAppId(app_info.package_name(),
                                         app_info.class_name());
    } else {
      app_id =
          ArcAppListPrefs::GetAppId(app_info.package_name(), app_info.action());
    }
    auto old_it = old_apps.find(app_id);
    if (old_it != old_apps.end()) {
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      // Use package name when display name is not specified.
      std::string name = app_info.package_name();
      if (!app_info.display_name().empty())
        name = app_info.display_name();
      apps_.push_back(std::make_unique<ArcKioskAppData>(
          app_id, app_info.package_name(), app_info.class_name(),
          app_info.action(), account_id, name));
      apps_.back()->LoadFromCache();
    }
    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(account_id);
  }

  std::vector<KioskAppDataBase*> old_apps_to_remove;
  for (auto& entry : old_apps)
    old_apps_to_remove.emplace_back(entry.second.get());
  ClearRemovedApps(old_apps_to_remove);

  NotifyKioskAppsChanged();
}
}  // namespace chromeos
