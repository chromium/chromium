// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_controller.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "ui/wm/core/wm_core_switches.h"

namespace ash {

namespace {

std::optional<KioskApp> WebAppById(const WebKioskAppManager& manager,
                                   const AccountId& account_id) {
  const WebKioskAppData* data = manager.GetAppByAccountId(account_id);
  if (!data) {
    return std::nullopt;
  }
  return KioskApp(KioskAppId::ForWebApp(account_id), data->name(), data->icon(),
                  data->install_url());
}

std::optional<KioskApp> ChromeAppById(const KioskChromeAppManager& manager,
                                      std::string_view chrome_app_id) {
  KioskChromeAppManager::App manager_app;
  if (!manager.GetApp(std::string(chrome_app_id), &manager_app)) {
    return std::nullopt;
  }
  return KioskApp(
      KioskAppId::ForChromeApp(chrome_app_id, manager_app.account_id),
      manager_app.name, manager_app.icon);
}

std::optional<KioskApp> ArcAppById(const ArcKioskAppManager& manager,
                                   const AccountId& account_id) {
  const ArcKioskAppData* data = manager.GetAppByAccountId(account_id);
  if (!data) {
    return std::nullopt;
  }
  return KioskApp(KioskAppId::ForArcApp(account_id), data->name(),
                  data->icon());
}

static KioskController* g_instance = nullptr;

}  // namespace

KioskController& KioskController::Get() {
  return CHECK_DEREF(g_instance);
}

KioskController::KioskController(user_manager::UserManager* user_manager) {
  CHECK(!g_instance);
  g_instance = this;

  user_manager_observation_.Observe(user_manager);
}

KioskController::~KioskController() {
  g_instance = nullptr;
}

std::vector<KioskApp> KioskController::GetApps() const {
  std::vector<KioskApp> apps;
  for (const KioskAppManagerBase::App& web_app : web_app_manager_.GetApps()) {
    apps.emplace_back(KioskAppId::ForWebApp(web_app.account_id), web_app.name,
                      web_app.icon, web_app.url);
  }
  for (const KioskAppManagerBase::App& chrome_app :
       chrome_app_manager_.GetApps()) {
    apps.emplace_back(
        KioskAppId::ForChromeApp(chrome_app.app_id, chrome_app.account_id),
        chrome_app.name, chrome_app.icon);
  }
  for (const KioskAppManagerBase::App& arc_app : arc_app_manager_.GetApps()) {
    apps.emplace_back(KioskAppId::ForArcApp(arc_app.account_id), arc_app.name,
                      arc_app.icon);
  }
  return apps;
}

std::optional<KioskApp> KioskController::GetAppById(
    const KioskAppId& app_id) const {
  switch (app_id.type) {
    case KioskAppType::kWebApp:
      return WebAppById(web_app_manager_, app_id.account_id);
    case KioskAppType::kChromeApp:
      return ChromeAppById(chrome_app_manager_, app_id.app_id.value());
    case KioskAppType::kArcApp:
      return ArcAppById(arc_app_manager_, app_id.account_id);
  }
}

std::optional<KioskApp> KioskController::GetAutoLaunchApp() const {
  if (const auto& web_account_id = web_app_manager_.GetAutoLaunchAccountId();
      web_account_id.is_valid()) {
    return WebAppById(web_app_manager_, web_account_id);
  } else if (chrome_app_manager_.IsAutoLaunchEnabled()) {
    std::string chrome_app_id = chrome_app_manager_.GetAutoLaunchApp();
    CHECK(!chrome_app_id.empty());
    return ChromeAppById(chrome_app_manager_, chrome_app_id);
  } else if (const auto& arc_account_id =
                 arc_app_manager_.GetAutoLaunchAccountId();
             arc_account_id.is_valid()) {
    return ArcAppById(arc_app_manager_, arc_account_id);
  }
  return std::nullopt;
}

void KioskController::OnUserLoggedIn(const user_manager::User& user) {
  if (!user.IsKioskType()) {
    return;
  }

  const AccountId& kiosk_app_account_id = user.GetAccountId();

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  const auto account = base::ranges::find(device_local_accounts,
                                          kiosk_app_account_id.GetUserEmail(),
                                          &policy::DeviceLocalAccount::user_id);
  std::string kiosk_app_id;
  if (account != device_local_accounts.end()) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: "
               << kiosk_app_account_id.GetUserEmail();
    CHECK_IS_TEST();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  // This happens in Web and Arc kiosks.
  if (!kiosk_app_id.empty()) {
    command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);
  }

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);

  // If restoring auto-launched kiosk session, make sure the app is marked
  // as auto-launched.
  if (command_line->HasSwitch(switches::kLoginUser) &&
      command_line->HasSwitch(switches::kAppAutoLaunched) &&
      !kiosk_app_id.empty()) {
    chrome_app_manager_.SetAppWasAutoLaunchedWithZeroDelay(kiosk_app_id);
  }
}

}  // namespace ash
