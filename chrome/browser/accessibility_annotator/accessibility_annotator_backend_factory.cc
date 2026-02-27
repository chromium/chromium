// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/sync/model/data_type_store_service.h"

// static
accessibility_annotator::AccessibilityAnnotatorBackend*
AccessibilityAnnotatorBackendFactory::GetForProfile(Profile* profile) {
  return static_cast<accessibility_annotator::AccessibilityAnnotatorBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccessibilityAnnotatorBackendFactory*
AccessibilityAnnotatorBackendFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotatorBackendFactory> instance;
  return instance.get();
}

AccessibilityAnnotatorBackendFactory::AccessibilityAnnotatorBackendFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorBackend",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

AccessibilityAnnotatorBackendFactory::~AccessibilityAnnotatorBackendFactory() =
    default;

std::unique_ptr<KeyedService>
AccessibilityAnnotatorBackendFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/486856790): Also check the kAccessibilityAnnotator feature
  // once setup.
  if (!base::FeatureList::IsEnabled(
          accessibility_annotator::kContentAnnotator)) {
    return nullptr;
  }
  return std::make_unique<
      accessibility_annotator::AccessibilityAnnotatorBackend>(
      chrome::GetChannel(), DataTypeStoreServiceFactory::GetForProfile(
                                Profile::FromBrowserContext(context))
                                ->GetStoreFactory());
}
