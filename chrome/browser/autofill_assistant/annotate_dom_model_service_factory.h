// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_ANNOTATE_DOM_MODEL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_ANNOTATE_DOM_MODEL_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {
class AnnotateDomModelService;
}  // namespace autofill_assistant

// Creates instances of |AnnotateDomMOdelService| per |BrowserContext|.
class AnnotateDomModelServiceFactory : public ProfileKeyedServiceFactory {
 public:
  AnnotateDomModelServiceFactory();
  ~AnnotateDomModelServiceFactory() override;

  // Gets the lazy instance of the factory.
  static AnnotateDomModelServiceFactory* GetInstance();

  // Gets the |AnnotateDomModelService| for the |browser_context|.
  //
  // Returns nullptr if the features that allows for annotating DOM is disabled.
  // Only available when the optimization guide service is.
  static autofill_assistant::AnnotateDomModelService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  //  CHROME_BROWSER_AUTOFILL_ASSISTANT_ANNOTATE_DOM_MODEL_SERVICE_FACTORY_H_
