// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_metrics_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
FamilyUserMetricsService* FamilyUserMetricsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FamilyUserMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
FamilyUserMetricsServiceFactory*
FamilyUserMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<FamilyUserMetricsServiceFactory> factory;
  return factory.get();
}

FamilyUserMetricsServiceFactory::FamilyUserMetricsServiceFactory()
    : ProfileKeyedServiceFactory(
          "FamilyUserMetricsServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(ChildUserServiceFactory::GetInstance());
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

FamilyUserMetricsServiceFactory::~FamilyUserMetricsServiceFactory() = default;

std::unique_ptr<KeyedService>
FamilyUserMetricsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FamilyUserMetricsService>(context);
}

}  // namespace ash
