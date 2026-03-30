// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementService;

class AccessibilityAnnotatorEnablementServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AccessibilityAnnotatorEnablementService* GetForProfile(
      Profile* profile);
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

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_FACTORY_H_
