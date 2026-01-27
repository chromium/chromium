// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_features.h"
#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service.h"
#include "chrome/browser/profiles/profile.h"

namespace accessibility_annotator {

// static
ContentAnnotatorService* ContentAnnotatorServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContentAnnotatorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContentAnnotatorServiceFactory* ContentAnnotatorServiceFactory::GetInstance() {
  static base::NoDestructor<ContentAnnotatorServiceFactory> instance;
  return instance.get();
}

ContentAnnotatorServiceFactory::ContentAnnotatorServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContentAnnotatorService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

ContentAnnotatorServiceFactory::~ContentAnnotatorServiceFactory() = default;

std::unique_ptr<KeyedService>
ContentAnnotatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kContentAnnotator)) {
    return nullptr;
  }
  return std::make_unique<ContentAnnotatorService>();
}

bool ContentAnnotatorServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace accessibility_annotator
