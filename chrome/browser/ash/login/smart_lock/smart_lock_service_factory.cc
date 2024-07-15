// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace ash {

namespace {

bool IsFeatureAllowed(content::BrowserContext* context) {
  return multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kSmartLock,
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace

// static
SmartLockServiceFactory* SmartLockServiceFactory::GetInstance() {
  static base::NoDestructor<SmartLockServiceFactory> instance;
  return instance.get();
}

// static
SmartLockService* SmartLockServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SmartLockService*>(
      SmartLockServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

// The keyed service uses the deprecated "EasyUnlockService" name to refer to
// Smart Lock.
SmartLockServiceFactory::SmartLockServiceFactory()
    : ProfileKeyedServiceFactory(
          "EasyUnlockService",
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
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
}

SmartLockServiceFactory::~SmartLockServiceFactory() = default;

KeyedService* SmartLockServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!context) {
    return nullptr;
  }

  if (!IsFeatureAllowed(context)) {
    return nullptr;
  }

  if (!features::IsCrossDeviceFeatureSuiteAllowed()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  if (!ProfileHelper::IsUserProfile(profile) ||
      !ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }

  SmartLockService* service = new SmartLockService(
      Profile::FromBrowserContext(context),
      secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
      device_sync::DeviceSyncClientFactory::GetForProfile(profile),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile));

  service->Initialize();
  return service;
}

void SmartLockServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  SmartLockService::RegisterProfilePrefs(registry);
#endif
}

bool SmartLockServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SmartLockServiceFactory::ServiceIsNULLWhileTesting() const {
  // Don't create the service for TestingProfile used in unit_tests because
  // SmartLockService uses ExtensionSystem::ready().Post, which expects
  // a MessageLoop that does not exit in many unit_tests cases.
  return true;
}

}  // namespace ash
