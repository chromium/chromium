// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace screen_ai {

class AXScreenAIAnnotator;

// Factory to get or create an instance of AXScreenAIAnnotator for a
// BrowserContext.
class AXScreenAIAnnotatorFactory : public ProfileKeyedServiceFactory {
 public:
  static screen_ai::AXScreenAIAnnotator* GetForBrowserContext(
      content::BrowserContext* context);

  static void EnsureExistsForBrowserContext(content::BrowserContext* context);

 private:
  friend class base::NoDestructor<AXScreenAIAnnotatorFactory>;
  static AXScreenAIAnnotatorFactory* GetInstance();

  AXScreenAIAnnotatorFactory();
  ~AXScreenAIAnnotatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_FACTORY_H_
