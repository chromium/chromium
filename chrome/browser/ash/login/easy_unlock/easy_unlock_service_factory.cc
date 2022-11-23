// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_signin.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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

  Profile* profile = Profile::FromBrowserContext(context);

  // TODO(b/227674947): When Sign in with Smart Lock is deprecated, remove the
  // check for ProfileHelper::IsSigninProfile() and do not instantiate
  // EasyUnlockServiceSignin here.
  // The SigninProfile is a special Profile used at the login screen.
  if (ProfileHelper::IsSigninProfile(profile)) {
    if (base::FeatureList::IsEnabled(features::kSmartLockSignInRemoved))
      return nullptr;

    if (!context->IsOffTheRecord())
      return nullptr;

    // There is only one "on the record" SigninProfile instantiated during the
    // login screen's lifetime, regardless of how many users have previously
    // signed into this device. This means only one EasyUnlockServiceSignin is
    // instantiated. This single EasyUnlockServiceSignin instance manages the
    // Smart Lock user flow for all user pods on the login screen.
    service = new EasyUnlockServiceSignin(
        profile,
        secure_channel::SecureChannelClientProvider::GetInstance()
            ->GetClient());
  } else if (!ProfileHelper::IsUserProfile(profile) ||
             !ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }

  if (!service) {
    // This is otherwise a "regular" Profile, i.e., a signed-in user (and
    // therefore this service will be serving the lock screen, not the login
    // screen). In contrast to EasyUnlockServiceSignin, an
    // EasyUnlockServiceRegular instance manages the Smart Lock user flow for
    // only one user.
    service = new EasyUnlockServiceRegular(
        Profile::FromBrowserContext(context),
        secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
        device_sync::DeviceSyncClientFactory::GetForProfile(profile),
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            profile));
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

}  // namespace ash
