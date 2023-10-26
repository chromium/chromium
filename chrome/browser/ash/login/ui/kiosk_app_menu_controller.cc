// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/kiosk_app_menu_controller.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_screen.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

KioskAppId ToKioskAppId(const KioskAppMenuEntry& menu_entry) {
  switch (menu_entry.type) {
    case KioskAppMenuEntry::AppType::kWebApp:
      return KioskAppId::ForWebApp(menu_entry.account_id);
    case KioskAppMenuEntry::AppType::kChromeApp:
      CHECK(menu_entry.chrome_app_id.has_value());
      return KioskAppId::ForChromeApp(menu_entry.chrome_app_id.value(),
                                      menu_entry.account_id);
    case KioskAppMenuEntry::AppType::kArcApp:
      return KioskAppId::ForArcApp(menu_entry.account_id);
  }
}

KioskAppMenuEntry::AppType ToMenuEntryType(KioskAppType type) {
  switch (type) {
    case KioskAppType::kWebApp:
      return KioskAppMenuEntry::AppType::kWebApp;
    case KioskAppType::kChromeApp:
      return KioskAppMenuEntry::AppType::kChromeApp;
    case KioskAppType::kArcApp:
      return KioskAppMenuEntry::AppType::kArcApp;
  }
}

gfx::ImageSkia IconOrDefault(gfx::ImageSkia icon) {
  return icon.isNull() ? *ui::ResourceBundle::GetSharedInstance()
                              .GetImageNamed(IDR_APP_DEFAULT_ICON)
                              .ToImageSkia()
                       : icon;
}

KioskAppMenuEntry ToMenuEntry(const KioskApp& app) {
  return KioskAppMenuEntry(ToMenuEntryType(app.id().type),
                           app.id().account_id.value(), app.id().app_id,
                           base::UTF8ToUTF16(app.name()),
                           IconOrDefault(app.icon()));
}

std::vector<KioskAppMenuEntry> BuildKioskAppMenuEntries() {
  std::vector<KioskAppMenuEntry> menu_entries;
  for (const KioskApp& app : KioskController::Get().GetApps()) {
    CHECK(app.id().account_id.has_value());
    menu_entries.push_back(ToMenuEntry(app));
  }
  return menu_entries;
}

}  // namespace

KioskAppMenuController::KioskAppMenuController() {
  kiosk_observations_.AddObservation(KioskAppManager::Get());
  kiosk_observations_.AddObservation(ArcKioskAppManager::Get());
  kiosk_observations_.AddObservation(WebKioskAppManager::Get());
}

KioskAppMenuController::~KioskAppMenuController() = default;

void KioskAppMenuController::OnKioskAppDataChanged(const std::string& app_id) {
  SendKioskApps();
  ConfigureKioskCallbacks();
}

void KioskAppMenuController::OnKioskAppDataLoadFailure(
    const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuController::OnKioskAppsSettingsChanged() {
  SendKioskApps();
}

void KioskAppMenuController::SendKioskApps() {
  if (!LoginScreenClientImpl::HasInstance()) {
    return;
  }

  KioskAppMenu::Get()->SetKioskApps(BuildKioskAppMenuEntries());
  KioskAppLaunchError::Error error = KioskAppLaunchError::Get();
  if (error == KioskAppLaunchError::Error::kNone) {
    return;
  }

  // Clear any old pending Kiosk launch errors
  KioskAppLaunchError::RecordMetricAndClear();

  LoginScreen::Get()->ShowKioskAppError(
      KioskAppLaunchError::GetErrorMessage(error));
}

void KioskAppMenuController::ConfigureKioskCallbacks() {
  KioskAppMenu::Get()->ConfigureKioskCallbacks(
      base::BindRepeating(&KioskAppMenuController::LaunchApp,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&KioskAppMenuController::OnMenuWillShow,
                          weak_factory_.GetWeakPtr()));
}

void KioskAppMenuController::LaunchApp(const KioskAppMenuEntry& menu_entry) {
  LoginDisplayHost::default_host()->StartKiosk(ToKioskAppId(menu_entry),
                                               /*is_auto_launch=*/false);
}

void KioskAppMenuController::OnMenuWillShow() {
  // Web app based kiosk app will want to load their icons.
  WebKioskAppManager::Get()->LoadIcons();
}

}  // namespace ash
