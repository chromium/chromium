// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif  // OS_CHROMEOS

namespace apps {

// static
bool AppServiceProxyFactory::IsAppServiceAvailableForProfile(Profile* profile) {
  if (!profile || profile->IsSystemProfile()) {
    return false;
  }

  // There is no AppServiceProxy for incognito profiles as they are ephemeral
  // and have no apps persisted inside them.
  //
  // A common pattern in incognito is to implicitly fall back to the associated
  // real profile. We do not do that here to avoid unintentionally leaking a
  // user's browsing data from incognito to an app. Clients of the App Service
  // should explicitly decide when it is and isn't appropriate to use the
  // associated real profile and pass that to this method.
#if defined(OS_CHROMEOS)
  // An exception on Chrome OS is the guest profile, which is incognito, but
  // can have apps within it.
  return (!chromeos::ProfileHelper::IsSigninProfile(profile) &&
          (!profile->IsOffTheRecord() || profile->IsGuestSession()));
#else
  return !profile->IsOffTheRecord();
#endif
}

// static
AppServiceProxy* AppServiceProxyFactory::GetForProfile(Profile* profile) {
  // TODO(https://crbug.com/1122463): remove this and convert back to a DCHECK
  // once we have audited and removed code paths that call here with a profile
  // that doesn't have an App Service.
  if (!IsAppServiceAvailableForProfile(profile)) {
    DVLOG(1) << "Called AppServiceProxyFactory::GetForProfile() on a profile "
                "which does not contain an AppServiceProxy. Please check "
                "whether this is appropriate as you may be leaking information "
                "out of this profile. Returning the AppServiceProxy attached "
                "to the parent profile instead.";
    // Fail tests that would trigger DumpWithoutCrashing.
    DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kTestType));
    base::debug::DumpWithoutCrashing();
  }

  auto* proxy = static_cast<AppServiceProxy*>(
      AppServiceProxyFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
  DCHECK_NE(nullptr, proxy);
  return proxy;
}

// static
AppServiceProxy* AppServiceProxyFactory::GetForProfileRedirectInIncognito(
    Profile* profile) {
  // TODO(https://crbug.com/1122463): replace this API and GetForProfile() with
  // one that allows clients to specify different levels of incognito tolerance,
  // where the default is to not leak out of incognito.
  if (!IsAppServiceAvailableForProfile(profile)) {
    profile = profile->GetOriginalProfile();
  }
  return GetForProfile(profile);
}

// static
AppServiceProxyFactory* AppServiceProxyFactory::GetInstance() {
  return base::Singleton<AppServiceProxyFactory>::get();
}

AppServiceProxyFactory::AppServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "AppServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#if defined(OS_CHROMEOS)
  DependsOn(guest_os::GuestOsRegistryServiceFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
#endif  // OS_CHROMEOS
}

AppServiceProxyFactory::~AppServiceProxyFactory() = default;

KeyedService* AppServiceProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppServiceProxy(Profile::FromBrowserContext(context));
}

content::BrowserContext* AppServiceProxyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsSystemProfile()) {
    return nullptr;
  }

#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  // We must have a proxy in guest mode to ensure default extension-based apps
  // are served.
  if (profile->IsGuestSession()) {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }
#endif  // OS_CHROMEOS

  // TODO(https://crbug.com/1122463): replace this with
  // BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context) once
  // all non-guest incognito accesses have been removed.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AppServiceProxyFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
