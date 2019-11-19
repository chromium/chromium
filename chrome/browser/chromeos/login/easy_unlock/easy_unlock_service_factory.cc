// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_signin_chromeos.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace chromeos {

namespace {

bool IsFeatureAllowed(content::BrowserContext* context) {
  return multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kSmartLock,
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace

// static
EasyUnlockServiceFactory* EasyUnlockServiceFactory::GetInstance() {
  return base::Singleton<EasyUnlockServiceFactory>::get();
}

// static
EasyUnlockService* EasyUnlockServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<EasyUnlockService*>(
      EasyUnlockServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

EasyUnlockServiceFactory::EasyUnlockServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EasyUnlockService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EasyUnlockTpmKeyManagerFactory::GetInstance());
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
}

EasyUnlockServiceFactory::~EasyUnlockServiceFactory() {}

KeyedService* EasyUnlockServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  EasyUnlockService* service = nullptr;

  if (!context)
    return nullptr;

  if (!IsFeatureAllowed(context))
    return nullptr;

  if (ProfileHelper::IsLockScreenAppProfile(
          Profile::FromBrowserContext(context))) {
    return nullptr;
  }

  if (ProfileHelper::IsSigninProfile(Profile::FromBrowserContext(context))) {
    if (!context->IsOffTheRecord())
      return nullptr;

    service = new EasyUnlockServiceSignin(
        Profile::FromBrowserContext(context),
        secure_channel::SecureChannelClientProvider::GetInstance()
            ->GetClient());
  }

  if (!service) {
    service = new EasyUnlockServiceRegular(
        Profile::FromBrowserContext(context),
        secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
        device_sync::DeviceSyncClientFactory::GetForProfile(
            Profile::FromBrowserContext(context)),
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            Profile::FromBrowserContext(context)));
  }

  service->Initialize();
  return service;
}

void EasyUnlockServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EasyUnlockService::RegisterProfilePrefs(registry);
#endif
}

content::BrowserContext* EasyUnlockServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (ProfileHelper::IsSigninProfile(Profile::FromBrowserContext(context))) {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }

  return context->IsOffTheRecord() ? nullptr : context;
}

bool EasyUnlockServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EasyUnlockServiceFactory::ServiceIsNULLWhileTesting() const {
  // Don't create the service for TestingProfile used in unit_tests because
  // EasyUnlockService uses ExtensionSystem::ready().Post, which expects
  // a MessageLoop that does not exit in many unit_tests cases.
  return true;
}

}  // namespace chromeos
