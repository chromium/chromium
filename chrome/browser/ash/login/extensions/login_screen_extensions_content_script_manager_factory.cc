// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/extensions/login_screen_extensions_content_script_manager_factory.h"

#include "chrome/browser/ash/login/extensions/login_screen_extensions_content_script_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry_factory.h"

namespace ash {

LoginScreenExtensionsContentScriptManager*
LoginScreenExtensionsContentScriptManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LoginScreenExtensionsContentScriptManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

LoginScreenExtensionsContentScriptManagerFactory*
LoginScreenExtensionsContentScriptManagerFactory::GetInstance() {
  static base::NoDestructor<LoginScreenExtensionsContentScriptManagerFactory>
      instance;
  return instance.get();
}

LoginScreenExtensionsContentScriptManagerFactory::
    LoginScreenExtensionsContentScriptManagerFactory()
    : ProfileKeyedServiceFactory(
          "LoginScreenExtensionsContentScriptManager",
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
}

LoginScreenExtensionsContentScriptManagerFactory::
    ~LoginScreenExtensionsContentScriptManagerFactory() = default;

std::unique_ptr<KeyedService> LoginScreenExtensionsContentScriptManagerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;
  if (!ProfileHelper::IsSigninProfile(profile)) {
    // The manager should only be created for the sign-in profile.
    return nullptr;
  }
  return std::make_unique<LoginScreenExtensionsContentScriptManager>(profile);
}

bool LoginScreenExtensionsContentScriptManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // The manager works in the background, regardless of whether something tried
  // to access it via the factory.
  return true;
}

}  // namespace ash
