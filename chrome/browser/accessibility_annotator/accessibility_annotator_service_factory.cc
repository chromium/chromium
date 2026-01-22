// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_features.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_service.h"
#include "chrome/browser/profiles/profile.h"

namespace accessibility_annotator {

// static
AccessibilityAnnotatorService*
AccessibilityAnnotatorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AccessibilityAnnotatorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccessibilityAnnotatorServiceFactory*
AccessibilityAnnotatorServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotatorServiceFactory> instance;
  return instance.get();
}

AccessibilityAnnotatorServiceFactory::AccessibilityAnnotatorServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

AccessibilityAnnotatorServiceFactory::~AccessibilityAnnotatorServiceFactory() =
    default;

std::unique_ptr<KeyedService>
AccessibilityAnnotatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kAccessibilityAnnotator)) {
    return nullptr;
  }
  return std::make_unique<AccessibilityAnnotatorService>(
      Profile::FromBrowserContext(context));
}

bool AccessibilityAnnotatorServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace accessibility_annotator
