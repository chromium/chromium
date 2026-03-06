// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotation_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/accessibility_annotator/core/accessibility_annotation_service.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/direct_server_entity_provider.h"
#include "content/public/browser/browser_context.h"

// static
accessibility_annotator::AccessibilityAnnotationService*
AccessibilityAnnotationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<accessibility_annotator::AccessibilityAnnotationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccessibilityAnnotationServiceFactory*
AccessibilityAnnotationServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotationServiceFactory> instance;
  return instance.get();
}

AccessibilityAnnotationServiceFactory::AccessibilityAnnotationServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotationService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AccessibilityAnnotationServiceFactory::
    ~AccessibilityAnnotationServiceFactory() = default;

std::unique_ptr<KeyedService>
AccessibilityAnnotationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          accessibility_annotator::kAccessibilityAnnotator)) {
    return nullptr;
  }

  return std::make_unique<
      accessibility_annotator::AccessibilityAnnotationService>(
      std::make_unique<accessibility_annotator::DirectServerEntityProvider>());
}
