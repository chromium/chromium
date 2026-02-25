// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_BACKEND_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_BACKEND_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace accessibility_annotator {
class AccessibilityAnnotatorBackend;
}

class AccessibilityAnnotatorBackendFactory : public ProfileKeyedServiceFactory {
 public:
  static accessibility_annotator::AccessibilityAnnotatorBackend* GetForProfile(
      Profile* profile);
  static AccessibilityAnnotatorBackendFactory* GetInstance();

  AccessibilityAnnotatorBackendFactory(
      const AccessibilityAnnotatorBackendFactory&) = delete;
  AccessibilityAnnotatorBackendFactory& operator=(
      const AccessibilityAnnotatorBackendFactory&) = delete;

 private:
  friend base::NoDestructor<AccessibilityAnnotatorBackendFactory>;

  AccessibilityAnnotatorBackendFactory();
  ~AccessibilityAnnotatorBackendFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_BACKEND_FACTORY_H_
