// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_factory.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sharesheet {

// static
SharesheetService* SharesheetServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SharesheetService*>(
      SharesheetServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
SharesheetServiceFactory* SharesheetServiceFactory::GetInstance() {
  return base::Singleton<SharesheetServiceFactory>::get();
}

SharesheetServiceFactory::SharesheetServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SharesheetService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

SharesheetServiceFactory::~SharesheetServiceFactory() = default;

KeyedService* SharesheetServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SharesheetService(Profile::FromBrowserContext(context));
}

content::BrowserContext* SharesheetServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsSystemProfile()) {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  // We allow sharing in guest mode or incognito mode..
  if (profile->IsGuestSession()) {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SharesheetServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace sharesheet
