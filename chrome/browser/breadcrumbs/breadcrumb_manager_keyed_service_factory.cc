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
    content::BrowserContext* context,
    bool create) {
  return static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

BreadcrumbManagerKeyedServiceFactory::BreadcrumbManagerKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "BreadcrumbManagerService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

BreadcrumbManagerKeyedServiceFactory::~BreadcrumbManagerKeyedServiceFactory() =
    default;

KeyedService* BreadcrumbManagerKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new breadcrumbs::BreadcrumbManagerKeyedService(
      context->IsOffTheRecord());
}
