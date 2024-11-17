// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // An exception on Chrome OS is the guest profile, which is incognito, but
  // can have apps within it.

  // Use OTR profile for Guest Session.
  if (profile->IsGuestSession()) {
    return profile->IsOffTheRecord();
  }

  return (!ash::ProfileHelper::IsSigninProfile(profile) &&
          !profile->IsOffTheRecord());
#else
  return !profile->IsOffTheRecord();
#endif
}

// static
AppServiceProxy* AppServiceProxyFactory::GetForProfile(Profile* profile) {
  // TODO(crbug.com/40146603): remove this and convert back to a DCHECK
  // once we have audited and removed code paths that call here with a profile
  // that doesn't have an App Service.
  if (!IsAppServiceAvailableForProfile(profile)) {
    // See comments in app_service_proxy_factory.h for how to handle profiles
    // with no AppServiceProxy. As an interim measure, we return the parent
    // profile in non-guest Incognito profiles.
    LOG(ERROR) << "Called AppServiceProxyFactory::GetForProfile() on a profile "
                  "which does not contain an AppServiceProxy";
    // Fail tests that would trigger DumpWithoutCrashing.
    DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kTestType));
    base::debug::DumpWithoutCrashing();
  }

  AppServiceProxy* proxy = static_cast<AppServiceProxy*>(
      AppServiceProxyFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
  DCHECK_NE(nullptr, proxy);
  return proxy;
}

// static
AppServiceProxyFactory* AppServiceProxyFactory::GetInstance() {
  static base::NoDestructor<AppServiceProxyFactory> instance;
  return instance.get();
}

AppServiceProxyFactory::AppServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "AppServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(ash::SystemWebAppManagerFactory::GetInstance());
  DependsOn(guest_os::GuestOsRegistryServiceFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

AppServiceProxyFactory::~AppServiceProxyFactory() = default;

KeyedService* AppServiceProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* proxy = new AppServiceProxy(Profile::FromBrowserContext(context));
  proxy->Initialize();
  return proxy;
}

content::BrowserContext* AppServiceProxyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsSystemProfile()) {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We must have a proxy in guest mode to ensure default extension-based apps
  // are served.
  if (profile->IsGuestSession()) {
    return profile->IsOffTheRecord()
               ? GetBrowserContextOwnInstanceInIncognito(context)
               : nullptr;
  }
  if (ash::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // TODO(crbug.com/40146603): replace this with
  // BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context) once
  // all non-guest incognito accesses have been removed.
  return GetBrowserContextRedirectedInIncognito(context);
}

bool AppServiceProxyFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
