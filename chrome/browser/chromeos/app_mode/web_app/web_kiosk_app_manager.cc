// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h>

#include <map>

#include "base/bind.h"
#include "chrome/browser/chromeos/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

namespace {
// This class is owned by ChromeBrowserMainPartsChromeos.
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

WebKioskAppManager::WebKioskAppManager()
    : auto_launch_account_id_(EmptyAccountId()) {
  CHECK(!g_web_kiosk_app_manager);  // Only one instance is allowed.
  g_web_kiosk_app_manager = this;
  UpdateAppsFromPolicy();
}

WebKioskAppManager::~WebKioskAppManager() {
  g_web_kiosk_app_manager = nullptr;
}

void WebKioskAppManager::GetApps(std::vector<App>* apps) const {
  apps->clear();
  apps->reserve(apps_.size());
  for (auto& web_app : apps_) {
    App app(*web_app);
    app.url = web_app->install_url();
    apps->push_back(std::move(app));
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
    std::unique_ptr<WebApplicationInfo> app_info) {
  for (auto& web_app : apps_) {
    if (web_app->account_id() == account_id) {
      web_app->UpdateFromWebAppInfo(std::move(app_info));
      return;
    }
  }
  NOTREACHED();
}

void WebKioskAppManager::AddAppForTesting(const AccountId& account_id,
                                          const GURL& install_url) {
  const std::string app_id = web_app::GenerateAppIdFromURL(install_url);
  apps_.push_back(
      std::make_unique<WebKioskAppData>(this, app_id, account_id, install_url));
}

void WebKioskAppManager::UpdateAppsFromPolicy() {
  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  std::map<std::string, std::unique_ptr<WebKioskAppData>> old_apps;
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
    if (account.type != policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP)
      continue;
    const AccountId account_id(AccountId::FromUserEmail(account.user_id));

    if (account.account_id == auto_login_account_id_from_settings) {
      auto_launch_account_id_ = account_id;
      int auto_launch_delay = 0;
      CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &auto_launch_delay);
      auto_launched_with_zero_delay_ = auto_launch_delay == 0;
    }
    GURL url(account.web_kiosk_app_info.url());
    std::string app_id = web_app::GenerateAppIdFromURL(url);

    auto old_it = old_apps.find(app_id);
    if (old_it != old_apps.end()) {
      // TODO(apotapchuk): Data fetcher will be created, will use it to
      // update previously not loaded data.
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      apps_.push_back(std::make_unique<WebKioskAppData>(
          this, app_id, account_id, std::move(url)));
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
