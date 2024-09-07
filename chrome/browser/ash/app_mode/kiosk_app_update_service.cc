// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"

namespace ash {

namespace {

// How low to wait after an update is available before we force a restart.
const int kForceRestartWaitTimeMs = 24 * 3600 * 1000;  // 24 hours.

}  // namespace

const char kKioskPrimaryAppInSessionUpdateHistogram[] =
    "Kiosk.ChromeApp.PrimaryAppInSessionUpdate";

KioskAppUpdateService::KioskAppUpdateService(
    Profile* profile,
    system::AutomaticRebootManager* automatic_reboot_manager)
    : profile_(profile), automatic_reboot_manager_(automatic_reboot_manager) {}

KioskAppUpdateService::~KioskAppUpdateService() = default;

void KioskAppUpdateService::Init(const std::string& app_id) {
  DCHECK(app_id_.empty());
  app_id_ = app_id;

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service) {
    service->AddUpdateObserver(this);
  }

  if (automatic_reboot_manager_) {
    automatic_reboot_manager_->AddObserver(this);
  }

  if (KioskChromeAppManager::IsInitialized()) {
    KioskChromeAppManager::Get()->AddObserver(this);
  }

  if (automatic_reboot_manager_->reboot_requested()) {
    OnRebootRequested(automatic_reboot_manager_->reboot_reason());
  }
}

void KioskAppUpdateService::StartAppUpdateRestartTimer() {
  base::UmaHistogramCounts100(kKioskPrimaryAppInSessionUpdateHistogram, 1);

  if (restart_timer_.IsRunning()) {
    return;
  }

  // Setup timer to force restart once the wait period expires.
  restart_timer_.Start(FROM_HERE, base::Milliseconds(kForceRestartWaitTimeMs),
                       this, &KioskAppUpdateService::ForceAppUpdateRestart);
}

void KioskAppUpdateService::ForceAppUpdateRestart() {
  // Force a chrome restart (not a logout or reboot) by closing all browsers.
  LOG(WARNING) << "Force closing all browsers to update kiosk app.";
  chrome::CloseAllBrowsersAndQuit();
}

void KioskAppUpdateService::Shutdown() {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service) {
    service->RemoveUpdateObserver(this);
  }
  if (KioskChromeAppManager::IsInitialized()) {
    KioskChromeAppManager::Get()->RemoveObserver(this);
  }
  if (automatic_reboot_manager_) {
    automatic_reboot_manager_->RemoveObserver(this);
  }
}

void KioskAppUpdateService::OnAppUpdateAvailable(
    const extensions::Extension* extension) {
  if (extension->id() != app_id_) {
    return;
  }

  // Clears cached app data so that it will be reloaded if update from app
  // does not finish in this run.
  KioskChromeAppManager::Get()->ClearAppData(app_id_);
  KioskChromeAppManager::Get()->UpdateAppDataFromProfile(app_id_, profile_,
                                                         extension);

  extensions::RuntimeEventRouter::DispatchOnRestartRequiredEvent(
      profile_, app_id_,
      extensions::api::runtime::OnRestartRequiredReason::kAppUpdate);

  StartAppUpdateRestartTimer();
}

void KioskAppUpdateService::OnRebootRequested(Reason reason) {
  extensions::api::runtime::OnRestartRequiredReason restart_reason =
      extensions::api::runtime::OnRestartRequiredReason::kNone;
  switch (reason) {
    case REBOOT_REASON_OS_UPDATE:
      restart_reason =
          extensions::api::runtime::OnRestartRequiredReason::kOsUpdate;
      break;
    case REBOOT_REASON_PERIODIC:
      restart_reason =
          extensions::api::runtime::OnRestartRequiredReason::kPeriodic;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown reboot reason=" << reason;
      return;
  }

  extensions::RuntimeEventRouter::DispatchOnRestartRequiredEvent(
      profile_, app_id_, restart_reason);
}

void KioskAppUpdateService::WillDestroyAutomaticRebootManager() {
  automatic_reboot_manager_->RemoveObserver(this);
  automatic_reboot_manager_ = nullptr;
}

void KioskAppUpdateService::OnKioskAppCacheUpdated(const std::string& app_id) {
  if (app_id != app_id_) {
    return;
  }

  extensions::RuntimeEventRouter::DispatchOnRestartRequiredEvent(
      profile_, app_id_,
      extensions::api::runtime::OnRestartRequiredReason::kAppUpdate);

  StartAppUpdateRestartTimer();
}

KioskAppUpdateServiceFactory::KioskAppUpdateServiceFactory()
    : ProfileKeyedServiceFactory(
          "KioskAppUpdateService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

KioskAppUpdateServiceFactory::~KioskAppUpdateServiceFactory() = default;

// static
KioskAppUpdateService* KioskAppUpdateServiceFactory::GetForProfile(
    Profile* profile) {
  // This should never be called unless we are running in forced app mode.
  DCHECK(IsRunningInForcedAppMode());
  if (!IsRunningInForcedAppMode()) {
    return nullptr;
  }

  return static_cast<KioskAppUpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
KioskAppUpdateServiceFactory* KioskAppUpdateServiceFactory::GetInstance() {
  return base::Singleton<KioskAppUpdateServiceFactory>::get();
}

std::unique_ptr<KeyedService>
KioskAppUpdateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<KioskAppUpdateService>(
      Profile::FromBrowserContext(context),
      g_browser_process->platform_part()->automatic_reboot_manager());
}

}  // namespace ash
