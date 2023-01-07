// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service_factory.h"

#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "content/public/browser/storage_partition.h"

// static
CartServiceFactory* CartServiceFactory::GetInstance() {
  return base::Singleton<CartServiceFactory>::get();
}

// static
CartService* CartServiceFactory::GetForProfile(Profile* profile) {
  // CartService is not supported on incognito.
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<CartService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CartServiceFactory::CartServiceFactory()
    : ProfileKeyedServiceFactory("ChromeCartService") {
  DependsOn(HistoryServiceFactory::GetInstance());
}

CartServiceFactory::~CartServiceFactory() = default;

KeyedService* CartServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  return new CartService(Profile::FromBrowserContext(context));
}
