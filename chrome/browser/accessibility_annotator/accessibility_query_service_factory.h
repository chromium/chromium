// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_QUERY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_QUERY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace accessibility_annotator {

class AccessibilityQueryService;

class AccessibilityQueryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AccessibilityQueryService* GetForProfile(Profile* profile);
  static AccessibilityQueryServiceFactory* GetInstance();

  AccessibilityQueryServiceFactory(const AccessibilityQueryServiceFactory&) =
      delete;
  AccessibilityQueryServiceFactory& operator=(
      const AccessibilityQueryServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AccessibilityQueryServiceFactory>;

  AccessibilityQueryServiceFactory();
  ~AccessibilityQueryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_QUERY_SERVICE_FACTORY_H_
