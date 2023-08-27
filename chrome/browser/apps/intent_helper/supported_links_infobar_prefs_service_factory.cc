// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {
bool IsEnabledForProfile(Profile* profile) {
  // Currently, the InfoBar only appears for web apps, so this service only
  // needs to run in profiles where web apps can be installed.
  return apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
             profile) &&
         web_app::AreWebAppsUserInstallable(profile);
}
}  // namespace

namespace apps {

// static
SupportedLinksInfoBarPrefsService*
SupportedLinksInfoBarPrefsServiceFactory::GetForProfile(Profile* profile) {
  if (!IsEnabledForProfile(profile))
    return nullptr;

  return static_cast<SupportedLinksInfoBarPrefsService*>(
      SupportedLinksInfoBarPrefsServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SupportedLinksInfoBarPrefsServiceFactory*
SupportedLinksInfoBarPrefsServiceFactory::GetInstance() {
  static base::NoDestructor<SupportedLinksInfoBarPrefsServiceFactory> instance;
  return instance.get();
}

SupportedLinksInfoBarPrefsServiceFactory::
    SupportedLinksInfoBarPrefsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SupportedLinksInfoBarPrefs",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

SupportedLinksInfoBarPrefsServiceFactory::
    ~SupportedLinksInfoBarPrefsServiceFactory() = default;

std::unique_ptr<KeyedService>
SupportedLinksInfoBarPrefsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SupportedLinksInfoBarPrefsService>(
      Profile::FromBrowserContext(context));
}

bool SupportedLinksInfoBarPrefsServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext*
SupportedLinksInfoBarPrefsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return IsEnabledForProfile(Profile::FromBrowserContext(context)) ? context
                                                                   : nullptr;
}

}  // namespace apps
