// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "content/public/browser/browser_context.h"

/// Factory
BitmapFetcherService* BitmapFetcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BitmapFetcherService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
BitmapFetcherServiceFactory* BitmapFetcherServiceFactory::GetInstance() {
  static base::NoDestructor<BitmapFetcherServiceFactory> instance;
  return instance.get();
}

BitmapFetcherServiceFactory::BitmapFetcherServiceFactory()
    : ProfileKeyedServiceFactory(
          "BitmapFetcherService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

BitmapFetcherServiceFactory::~BitmapFetcherServiceFactory() = default;

std::unique_ptr<KeyedService>
BitmapFetcherServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  return std::make_unique<BitmapFetcherService>(context);
}
