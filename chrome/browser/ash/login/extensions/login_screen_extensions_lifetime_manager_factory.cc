// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/extensions/login_screen_extensions_lifetime_manager_factory.h"

#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/extensions/login_screen_extensions_lifetime_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/session_manager/core/session_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/process_manager_factory.h"

namespace ash {

LoginScreenExtensionsLifetimeManager*
LoginScreenExtensionsLifetimeManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<LoginScreenExtensionsLifetimeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

LoginScreenExtensionsLifetimeManagerFactory*
LoginScreenExtensionsLifetimeManagerFactory::GetInstance() {
  static base::NoDestructor<LoginScreenExtensionsLifetimeManagerFactory>
      instance;
  return instance.get();
}

LoginScreenExtensionsLifetimeManagerFactory::
    LoginScreenExtensionsLifetimeManagerFactory()
    : ProfileKeyedServiceFactory(
          "LoginScreenExtensionsLifetimeManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(extensions::ProcessManagerFactory::GetInstance());
}

LoginScreenExtensionsLifetimeManagerFactory::
    ~LoginScreenExtensionsLifetimeManagerFactory() = default;

std::unique_ptr<KeyedService> LoginScreenExtensionsLifetimeManagerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  // Exit early in unit tests that don't initialize prerequisites for the
  // manager.
  if (!session_manager::SessionManager::Get()) {
    CHECK_IS_TEST();
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;
  if (!ProfileHelper::IsSigninProfile(profile)) {
    // The manager should only be created for the sign-in profile.
    return nullptr;
  }
  return std::make_unique<LoginScreenExtensionsLifetimeManager>(profile);
}

bool LoginScreenExtensionsLifetimeManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // The manager works in the background, regardless of whether something tried
  // to access it via the factory.
  return true;
}

}  // namespace ash
