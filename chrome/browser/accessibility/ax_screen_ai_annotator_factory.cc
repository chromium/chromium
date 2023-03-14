// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_screen_ai_annotator_factory.h"

#include "chrome/browser/accessibility/ax_screen_ai_annotator.h"
#include "content/public/browser/browser_context.h"

namespace screen_ai {

// static
screen_ai::AXScreenAIAnnotator*
AXScreenAIAnnotatorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<screen_ai::AXScreenAIAnnotator*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AXScreenAIAnnotatorFactory* AXScreenAIAnnotatorFactory::GetInstance() {
  static base::NoDestructor<AXScreenAIAnnotatorFactory> instance;
  return instance.get();
}

// static
void AXScreenAIAnnotatorFactory::EnsureExistsForBrowserContext(
    content::BrowserContext* context) {
  GetForBrowserContext(context);
}

AXScreenAIAnnotatorFactory::AXScreenAIAnnotatorFactory()
    : ProfileKeyedServiceFactory(
          "AXScreenAIAnnotator",
          // Incognito profiles should use their own instance.
          ProfileSelections::BuildForRegularAndIncognito()) {}

AXScreenAIAnnotatorFactory::~AXScreenAIAnnotatorFactory() = default;

KeyedService* AXScreenAIAnnotatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new screen_ai::AXScreenAIAnnotator(context);
}

// static
void AXScreenAIAnnotatorFactory::EnsureFactoryBuilt() {
  AXScreenAIAnnotatorFactory::GetInstance();
}

}  // namespace screen_ai
