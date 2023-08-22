// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_proxy_service_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/cups_proxy_service_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// static
CupsProxyServiceManagerFactory* CupsProxyServiceManagerFactory::GetInstance() {
  static base::NoDestructor<CupsProxyServiceManagerFactory> factory;
  return factory.get();
}

// static
CupsProxyServiceManager* CupsProxyServiceManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CupsProxyServiceManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CupsProxyServiceManagerFactory::CupsProxyServiceManagerFactory()
    : ProfileKeyedServiceFactory(
          "CupsProxyServiceManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // We do not need an instance of CupsProxyServiceManager on the
              // lockscreen.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

CupsProxyServiceManagerFactory::~CupsProxyServiceManagerFactory() = default;

std::unique_ptr<KeyedService>
CupsProxyServiceManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Only create the service for the primary user.
  Profile* profile = Profile::FromBrowserContext(context);
  auto* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (!user_manager::UserManager::Get()->IsPrimaryUser(user)) {
    return nullptr;
  }

  return std::make_unique<CupsProxyServiceManager>(profile);
}

bool CupsProxyServiceManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool CupsProxyServiceManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
