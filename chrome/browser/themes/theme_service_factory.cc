// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "ui/base/mojom/themes.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/themes/theme_helper_win.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/themes/theme_service_aura_linux.h"
#endif

namespace {

const ThemeHelper& GetThemeHelper() {
#if BUILDFLAG(IS_WIN)
  using ThemeHelper = ThemeHelperWin;
#endif

  static base::NoDestructor<std::unique_ptr<ThemeHelper>> theme_helper(
      std::make_unique<ThemeHelper>());
  return **theme_helper;
}

}  // namespace

// static
ThemeService* ThemeServiceFactory::GetForProfile(Profile* profile) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("loading"),
              "ThemeServiceFactory::GetForProfile");
  return static_cast<ThemeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ThemeService* ThemeServiceFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<ThemeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
const extensions::Extension* ThemeServiceFactory::GetThemeForProfile(
    Profile* profile) {
  ThemeService* theme_service = GetForProfile(profile);
  if (!theme_service->UsingExtensionTheme()) {
    return nullptr;
  }

  return extensions::ExtensionRegistry::Get(profile)
      ->enabled_extensions()
      .GetByID(theme_service->GetThemeID());
}

// static
ThemeServiceFactory* ThemeServiceFactory::GetInstance() {
  static base::NoDestructor<ThemeServiceFactory> instance;
  return instance.get();
}

ThemeServiceFactory::ThemeServiceFactory()
    : ProfileKeyedServiceFactory(
          "ThemeService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::ExtensionRegistrarFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ChromeExtensionSystemFactory::GetInstance());
  DependsOn(NtpCustomBackgroundServiceFactory::GetInstance());
}

ThemeServiceFactory::~ThemeServiceFactory() = default;

std::unique_ptr<KeyedService>
ThemeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
#if BUILDFLAG(IS_LINUX)
  using ThemeService = ThemeServiceAuraLinux;
#endif

  auto provider = std::make_unique<ThemeService>(static_cast<Profile*>(profile),
                                                 GetThemeHelper());
  provider->Init();
  return provider;
}

bool ThemeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

void ThemeServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* browser_context) {
  Profile::FromBrowserContext(browser_context)->set_theme_service(nullptr);
  BrowserContextKeyedServiceFactory::BrowserContextDestroyed(browser_context);
}
