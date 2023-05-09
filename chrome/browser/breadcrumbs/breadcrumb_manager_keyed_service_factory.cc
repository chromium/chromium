// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "content/public/browser/browser_context.h"

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
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

BreadcrumbManagerKeyedServiceFactory::BreadcrumbManagerKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "BreadcrumbManagerService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

BreadcrumbManagerKeyedServiceFactory::~BreadcrumbManagerKeyedServiceFactory() =
    default;

KeyedService* BreadcrumbManagerKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new breadcrumbs::BreadcrumbManagerKeyedService(
      context->IsOffTheRecord());
}
