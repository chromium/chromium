// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/page_image_service/image_service_factory.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_impl.h"

namespace page_image_service {

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
    : ProfileKeyedServiceFactory(
          "ImageService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(RemoteSuggestionsServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

ImageServiceFactory::~ImageServiceFactory() = default;

std::unique_ptr<KeyedService>
ImageServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ImageServiceImpl>(
      TemplateURLServiceFactory::GetForProfile(profile),
      RemoteSuggestionsServiceFactory::GetForProfile(
          profile, /*create_if_necessary=*/true),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      std::make_unique<ChromeAutocompleteSchemeClassifier>(profile));
}

// static
void ImageServiceFactory::EnsureFactoryBuilt() {
  GetInstance();
}

}  // namespace page_image_service
