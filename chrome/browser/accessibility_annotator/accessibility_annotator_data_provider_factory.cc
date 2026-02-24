// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_data_provider_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_data_provider_impl.h"

// static
accessibility_annotator::AccessibilityAnnotatorDataProvider*
AccessibilityAnnotatorDataProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<
      accessibility_annotator::AccessibilityAnnotatorDataProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccessibilityAnnotatorDataProviderFactory*
AccessibilityAnnotatorDataProviderFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotatorDataProviderFactory> instance;
  return instance.get();
}

AccessibilityAnnotatorDataProviderFactory::
    AccessibilityAnnotatorDataProviderFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorDataProvider",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AccessibilityAnnotatorDataProviderFactory::
    ~AccessibilityAnnotatorDataProviderFactory() = default;

std::unique_ptr<KeyedService> AccessibilityAnnotatorDataProviderFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<
      accessibility_annotator::AccessibilityAnnotatorDataProviderImpl>();
}
