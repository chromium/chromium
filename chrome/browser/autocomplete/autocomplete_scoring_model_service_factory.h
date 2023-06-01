// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class AutocompleteScoringModelService;
class Profile;

// A factory to create a unique `AutocompleteScoringModelService` per profile.
// Has dependency on `OptimizationGuideKeyedServiceFactory`.
class AutocompleteScoringModelServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets the singleton instance of
  // `AutocompleteScoringModelServiceFactory`.
  static AutocompleteScoringModelServiceFactory* GetInstance();

  // Gets the `AutocompleteScoringModelService` for the profile.
  static AutocompleteScoringModelService* GetForProfile(Profile* profile);

  // Disallow copy/assign.
  AutocompleteScoringModelServiceFactory(
      const AutocompleteScoringModelServiceFactory&) = delete;
  AutocompleteScoringModelServiceFactory& operator=(
      const AutocompleteScoringModelServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AutocompleteScoringModelServiceFactory>;

  AutocompleteScoringModelServiceFactory();
  ~AutocompleteScoringModelServiceFactory() override;

  // `BrowserContextKeyedServiceFactory` overrides.
  //
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
