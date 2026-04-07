// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace accessibility_annotator {
class AccessibilityAnnotatorFirstRunService;
}

namespace content {
class BrowserContext;
}

class Profile;

class AccessibilityAnnotatorFirstRunServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static accessibility_annotator::AccessibilityAnnotatorFirstRunService*
  GetForProfile(Profile* profile);
  static AccessibilityAnnotatorFirstRunServiceFactory* GetInstance();

  AccessibilityAnnotatorFirstRunServiceFactory(
      const AccessibilityAnnotatorFirstRunServiceFactory&) = delete;
  AccessibilityAnnotatorFirstRunServiceFactory& operator=(
      const AccessibilityAnnotatorFirstRunServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AccessibilityAnnotatorFirstRunServiceFactory>;
  AccessibilityAnnotatorFirstRunServiceFactory();
  ~AccessibilityAnnotatorFirstRunServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_SERVICE_FACTORY_H_
