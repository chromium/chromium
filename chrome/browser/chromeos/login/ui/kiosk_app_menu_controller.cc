// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/kiosk_app_menu_controller.h"

#include <utility>

#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_screen.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "content/public/browser/notification_service.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

KioskAppMenuController::KioskAppMenuController() {
  kiosk_observer_.Add(KioskAppManager::Get());
  kiosk_observer_.Add(ArcKioskAppManager::Get());
  kiosk_observer_.Add(WebKioskAppManager::Get());
}

KioskAppMenuController::~KioskAppMenuController() = default;

void KioskAppMenuController::OnKioskAppDataChanged(const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuController::OnKioskAppDataLoadFailure(
    const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuController::OnKioskAppsSettingsChanged() {
  SendKioskApps();
}

void KioskAppMenuController::SendKioskApps() {
  if (!LoginScreenClient::HasInstance())
    return;

  std::vector<ash::KioskAppMenuEntry> output;

  const gfx::ImageSkia default_icon = *ui::ResourceBundle::GetSharedInstance()
                                           .GetImageNamed(IDR_APP_DEFAULT_ICON)
                                           .ToImageSkia();

  const std::vector<KioskAppManagerBase*> kiosk_managers = {
      KioskAppManager::Get(), WebKioskAppManager::Get(),
      ArcKioskAppManager::Get()};
  for (auto* manager : kiosk_managers) {
    std::vector<KioskAppManagerBase::App> apps;
    manager->GetApps(&apps);
    for (const auto& app : apps) {
      ash::KioskAppMenuEntry menu_entry;
      menu_entry.app_id = app.app_id;
      menu_entry.account_id = app.account_id;
      menu_entry.name = base::UTF8ToUTF16(app.name);
      menu_entry.icon = app.icon.isNull() ? default_icon : app.icon;
      output.push_back(std::move(menu_entry));
    }
  }

  ash::KioskAppMenu::Get()->SetKioskApps(
      output, base::BindRepeating(&KioskAppMenuController::LaunchApp,
                                  weak_factory_.GetWeakPtr()));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  KioskAppLaunchError::Error error = KioskAppLaunchError::Get();
  if (error == KioskAppLaunchError::NONE)
    return;

  // Clear any old pending Kiosk launch errors
  KioskAppLaunchError::RecordMetricAndClear();

  ash::LoginScreen::Get()->ShowKioskAppError(
      KioskAppLaunchError::GetErrorMessage(error));
}

void KioskAppMenuController::LaunchApp(const ash::KioskAppMenuEntry& app) {
  auto* host = chromeos::LoginDisplayHost::default_host();
  if (!app.account_id.is_valid())
    return;

  policy::DeviceLocalAccount::Type type;
  if (!policy::IsDeviceLocalAccountUser(app.account_id.GetUserEmail(), &type)) {
    NOTREACHED();
    return;
  }
  switch (type) {
    case policy::DeviceLocalAccount::TYPE_KIOSK_APP:
      host->StartAppLaunch(app.app_id, /*diagnostic_mode=*/false,
                           /*is_auto_launch=*/false);
      return;
    case policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP:
      host->StartArcKiosk(app.account_id);
      return;
    case policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP:
      host->StartWebKiosk(app.account_id);
      return;
    default:
      break;
  }
  NOTREACHED();
}

}  // namespace chromeos
