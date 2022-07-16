// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_session_ash.h"

#include "ash/public/cpp/accessibility_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/ash/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Start the floating accessibility menu in ash-chrome if the
// `FloatingAccessibilityMenuEnabled` policy is enabled.
void StartFloatingAccessibilityMenu() {
  ash::AccessibilityController* accessibility_controller =
      ash::AccessibilityController::Get();
  if (accessibility_controller)
    accessibility_controller->ShowFloatingMenuIfEnabled();
}

}  // namespace

AppSessionAsh::~AppSessionAsh() = default;

void AppSessionAsh::Init(Profile* profile, const std::string& app_id) {
  chromeos::AppSession::Init(profile, app_id);
  StartFloatingAccessibilityMenu();
  InitKioskAppUpdateService(profile, app_id);
  SetRebootAfterUpdateIfNecessary();
}

void AppSessionAsh::InitForWebKiosk(Browser* browser) {
  chromeos::AppSession::InitForWebKiosk(browser);
  StartFloatingAccessibilityMenu();
}

void AppSessionAsh::InitForWebKioskWithLacros(Profile* profile) {
  SetProfile(profile);
  CreateBrowserWindowHandler(nullptr);
  StartFloatingAccessibilityMenu();
}

void AppSessionAsh::InitKioskAppUpdateService(Profile* profile,
                                              const std::string& app_id) {
  // Set the app_id for the current instance of KioskAppUpdateService.
  ash::KioskAppUpdateService* update_service =
      ash::KioskAppUpdateServiceFactory::GetForProfile(profile);
  DCHECK(update_service);
  if (update_service)
    update_service->Init(app_id);

  // Start to monitor external update from usb stick.
  KioskAppManager::Get()->MonitorKioskExternalUpdate();
}

void AppSessionAsh::SetRebootAfterUpdateIfNecessary() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kRebootAfterUpdate, true);
    KioskModeIdleAppNameNotification::Initialize();
  }
}

}  // namespace ash
