// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_service/image_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/image_service/image_service.h"

namespace image_service {

// static
ImageService* ImageServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ImageService*>(
      GetInstance().GetServiceForBrowserContext(browser_context, true));
}

// static
ImageServiceFactory& ImageServiceFactory::GetInstance() {
  static base::NoDestructor<ImageServiceFactory> instance;
  return *instance;
}

ImageServiceFactory::ImageServiceFactory()
    : ProfileKeyedServiceFactory("ImageService") {
  DependsOn(SyncServiceFactory::GetInstance());
}

ImageServiceFactory::~ImageServiceFactory() = default;

KeyedService* ImageServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  return new ImageService(
      std::make_unique<ChromeAutocompleteProviderClient>(profile),
      SyncServiceFactory::GetForProfile(profile));
}

}  // namespace image_service
