// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_factory.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"

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
    : ProfileKeyedServiceFactory(
          "SharesheetService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
#if BUILDFLAG(IS_CHROMEOS_ASH)
              // We allow sharing in guest mode or incognito mode..
              .WithGuest(ProfileSelection::kOwnInstance)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

SharesheetServiceFactory::~SharesheetServiceFactory() = default;

KeyedService* SharesheetServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return new SharesheetService(profile);
}

bool SharesheetServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace sharesheet
