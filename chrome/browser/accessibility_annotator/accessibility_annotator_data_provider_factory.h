// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace accessibility_annotator {
class AccessibilityAnnotatorDataProvider;
}

class AccessibilityAnnotatorDataProviderFactory
    : public ProfileKeyedServiceFactory {
 public:
  static accessibility_annotator::AccessibilityAnnotatorDataProvider*
  GetForProfile(Profile* profile);
  static AccessibilityAnnotatorDataProviderFactory* GetInstance();

  AccessibilityAnnotatorDataProviderFactory(
      const AccessibilityAnnotatorDataProviderFactory&) = delete;
  AccessibilityAnnotatorDataProviderFactory& operator=(
      const AccessibilityAnnotatorDataProviderFactory&) = delete;

 private:
  friend base::NoDestructor<AccessibilityAnnotatorDataProviderFactory>;

  AccessibilityAnnotatorDataProviderFactory();
  ~AccessibilityAnnotatorDataProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_DATA_PROVIDER_FACTORY_H_
