// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace accessibility_annotator {

class AccessibilityAnnotatorService;

class AccessibilityAnnotatorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AccessibilityAnnotatorService* GetForProfile(Profile* profile);
  static AccessibilityAnnotatorServiceFactory* GetInstance();

  AccessibilityAnnotatorServiceFactory(
      const AccessibilityAnnotatorServiceFactory&) = delete;
  AccessibilityAnnotatorServiceFactory& operator=(
      const AccessibilityAnnotatorServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AccessibilityAnnotatorServiceFactory>;

  AccessibilityAnnotatorServiceFactory();
  ~AccessibilityAnnotatorServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_SERVICE_FACTORY_H_
