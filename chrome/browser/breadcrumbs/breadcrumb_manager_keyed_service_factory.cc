// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "content/public/browser/browser_context.h"

namespace {

// Returns a ProfileSelections indicating that BreadcrumbManagerKeyedService
// should only be built for regular and incognito profiles, not for the system
// profile or CrOS's irregular profiles (e.g., login and lock screen profiles).
ProfileSelections BreadcrumbManagerProfileSelections() {
  if (!breadcrumbs::IsEnabled())
    return ProfileSelections::BuildNoProfilesSelected();

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOwnInstance)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

}  // namespace

// static
BreadcrumbManagerKeyedServiceFactory*
BreadcrumbManagerKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<BreadcrumbManagerKeyedServiceFactory> instance;
  return instance.get();
}

// static
breadcrumbs::BreadcrumbManagerKeyedService*
BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

BreadcrumbManagerKeyedServiceFactory::BreadcrumbManagerKeyedServiceFactory()
    : ProfileKeyedServiceFactory("BreadcrumbManagerService",
                                 BreadcrumbManagerProfileSelections()) {}

BreadcrumbManagerKeyedServiceFactory::~BreadcrumbManagerKeyedServiceFactory() =
    default;

KeyedService* BreadcrumbManagerKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new breadcrumbs::BreadcrumbManagerKeyedService(
      context->IsOffTheRecord());
}

bool BreadcrumbManagerKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
