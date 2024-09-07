// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_controller.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "build/buildflag.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_policy_handler.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

namespace {

static KioskController* g_instance = nullptr;

}  // namespace

// static
KioskController& KioskController::Get() {
  return CHECK_DEREF(g_instance);
}

KioskController::KioskController() {
  CHECK(!g_instance);
  g_instance = this;
}

KioskController::~KioskController() {
  g_instance = nullptr;
}

// static
void KioskController::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  KioskChromeAppManager::RegisterLocalStatePrefs(registry);
  WebKioskAppManager::RegisterPrefs(registry);
  if (ash::features::IsIsolatedWebAppKioskEnabled()) {
    KioskIwaManager::RegisterPrefs(registry);
  }
  KioskCryptohomeRemover::RegisterPrefs(registry);

  kiosk_vision::RegisterLocalStatePrefs(registry);
  policy::DeviceWeeklyScheduledSuspendPolicyHandler::RegisterLocalStatePrefs(
      registry);
}

// static
void KioskController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  KioskChromeAppManager::RegisterProfilePrefs(registry);
  KioskSystemSession::RegisterProfilePrefs(registry);
}

}  // namespace ash
