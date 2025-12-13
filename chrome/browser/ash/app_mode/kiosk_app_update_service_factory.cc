// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_update_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/singleton.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/updater/extension_updater_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace ash {

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
  DependsOn(extensions::ExtensionUpdaterFactory::GetInstance());
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
