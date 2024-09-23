// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service_factory.h"

#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "content/public/browser/storage_partition.h"

namespace {

std::unique_ptr<KeyedService> BuildCartService(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(!profile->IsOffTheRecord());
  return std::make_unique<CartService>(profile);
}

}  // namespace

// static
CartServiceFactory* CartServiceFactory::GetInstance() {
  static base::NoDestructor<CartServiceFactory> instance;
  return instance.get();
}

// static
CartService* CartServiceFactory::GetForProfile(Profile* profile) {
  // CartService is not supported on incognito.
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<CartService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
CartServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildCartService);
}

CartServiceFactory::CartServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromeCartService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

CartServiceFactory::~CartServiceFactory() = default;

std::unique_ptr<KeyedService>
CartServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildCartService(context);
}
