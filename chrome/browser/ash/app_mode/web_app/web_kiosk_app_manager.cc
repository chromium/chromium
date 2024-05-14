// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_update_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "url/gurl.h"

namespace ash {

namespace {
// This class is owned by `ChromeBrowserMainPartsAsh`.
static WebKioskAppManager* g_web_kiosk_app_manager = nullptr;
}  // namespace

// static
const char WebKioskAppManager::kWebKioskDictionaryName[] = "web-kiosk";

// static
void WebKioskAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kWebKioskDictionaryName);
}

// static
bool WebKioskAppManager::IsInitialized() {
  return g_web_kiosk_app_manager;
}

// static
WebKioskAppManager* WebKioskAppManager::Get() {
  CHECK(g_web_kiosk_app_manager);
  return g_web_kiosk_app_manager;
}

// static
KioskAppManagerBase::App WebKioskAppManager::CreateAppByData(
    const WebKioskAppData& data) {
  auto app = KioskAppManagerBase::App(data);
  app.url = data.install_url();
  return app;
}

WebKioskAppManager::WebKioskAppManager()
    : auto_launch_account_id_(EmptyAccountId()) {
  CHECK(!g_web_kiosk_app_manager);  // Only one instance is allowed.
  g_web_kiosk_app_manager = this;
  UpdateAppsFromPolicy();
}

WebKioskAppManager::~WebKioskAppManager() {
  g_web_kiosk_app_manager = nullptr;
}

std::vector<WebKioskAppManager::App> WebKioskAppManager::GetApps() const {
  std::vector<App> apps;
  apps.reserve(apps_.size());
  for (const auto& manager_app : apps_) {
    App app(*manager_app);
    app.url = manager_app->install_url();
    apps.push_back(std::move(app));
  }
  return apps;
}

void WebKioskAppManager::LoadIcons() {
  for (auto& web_app : apps_) {
    web_app->LoadIcon();
  }
}

const AccountId& WebKioskAppManager::GetAutoLaunchAccountId() const {
  return auto_launch_account_id_;
}

const WebKioskAppData* WebKioskAppManager::GetAppByAccountId(
    const AccountId& account_id) const {
  for (const auto& web_app : apps_) {
    if (web_app->account_id() == account_id) {
      return web_app.get();
    }
  }
  return nullptr;
}

void WebKioskAppManager::UpdateAppByAccountId(
    const AccountId& account_id,
    const web_app::WebAppInstallInfo& app_info) {
  for (auto& web_app : apps_) {
    if (web_app->account_id() == account_id) {
      web_app->UpdateFromWebAppInfo(app_info);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void WebKioskAppManager::UpdateAppByAccountId(
    const AccountId& account_id,
    const std::string& title,
    const GURL& start_url,
    const web_app::IconBitmaps& icon_bitmaps) {
  for (auto& web_app : apps_) {
    if (web_app->account_id() == account_id) {
      web_app->UpdateAppInfo(title, start_url, icon_bitmaps);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void WebKioskAppManager::AddAppForTesting(const AccountId& account_id,
                                          const GURL& install_url) {
  const std::string app_id =
      web_app::GenerateAppId(/*manifest_id_path=*/std::nullopt, install_url);
  apps_.push_back(std::make_unique<WebKioskAppData>(
      this, app_id, account_id, install_url, /*title*/ std::string(),
      /*icon_url*/ GURL()));
  NotifyKioskAppsChanged();
}

void WebKioskAppManager::OnKioskSessionStarted(const KioskAppId& app_id) {
  NotifySessionInitialized();
}

void WebKioskAppManager::UpdateAppsFromPolicy() {
  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  std::map<std::string, std::unique_ptr<WebKioskAppData>> old_apps;
  for (auto& app : apps_) {
    old_apps[app->app_id()] = std::move(app);
  }
  apps_.clear();
  auto_launch_account_id_.clear();
  auto_launched_with_zero_delay_ = false;
  std::string auto_login_account_id_from_settings;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id_from_settings);

  // Re-populates `apps_` and reuses existing apps when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (auto account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccountType::kWebKioskApp) {
      continue;
    }
    const AccountId account_id(AccountId::FromUserEmail(account.user_id));

    if (account.account_id == auto_login_account_id_from_settings) {
      auto_launch_account_id_ = account_id;
      int auto_launch_delay = 0;
      CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &auto_launch_delay);
      auto_launched_with_zero_delay_ = auto_launch_delay == 0;
    }

    GURL url(account.web_kiosk_app_info.url());
    std::string title = account.web_kiosk_app_info.title();
    GURL icon_url = GURL(account.web_kiosk_app_info.icon_url());

    std::string app_id =
        web_app::GenerateAppId(/*manifest_id_path=*/std::nullopt, url);

    auto old_it = old_apps.find(app_id);
    if (old_it != old_apps.end()) {
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      apps_.push_back(std::make_unique<WebKioskAppData>(
          this, app_id, account_id, std::move(url), title,
          std::move(icon_url)));
      apps_.back()->LoadFromCache();
    }

    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(account_id);
  }

  std::vector<KioskAppDataBase*> old_apps_to_remove;
  for (auto& entry : old_apps) {
    old_apps_to_remove.emplace_back(entry.second.get());
  }
  ClearRemovedApps(old_apps_to_remove);
  NotifyKioskAppsChanged();
}

void WebKioskAppManager::StartObservingAppUpdate(Profile* profile,
                                                 const AccountId& account_id) {
  app_update_observer_ =
      std::make_unique<WebKioskAppUpdateObserver>(profile, account_id);
}

}  // namespace ash
