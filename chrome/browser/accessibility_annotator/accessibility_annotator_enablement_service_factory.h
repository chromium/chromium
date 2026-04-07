// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace accessibility_annotator {
class AccessibilityAnnotatorEnablementService;
}

class Profile;

class AccessibilityAnnotatorEnablementServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static accessibility_annotator::AccessibilityAnnotatorEnablementService*
  GetForProfile(Profile* profile);
  static AccessibilityAnnotatorEnablementServiceFactory* GetInstance();

  AccessibilityAnnotatorEnablementServiceFactory(
      const AccessibilityAnnotatorEnablementServiceFactory&) = delete;
  AccessibilityAnnotatorEnablementServiceFactory& operator=(
      const AccessibilityAnnotatorEnablementServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AccessibilityAnnotatorEnablementServiceFactory>;

  AccessibilityAnnotatorEnablementServiceFactory();
  ~AccessibilityAnnotatorEnablementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_FACTORY_H_
