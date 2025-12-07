// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_OPTIMIZATION_GUIDE_DECIDER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_OPTIMIZATION_GUIDE_DECIDER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class KeyedService;
class Profile;

namespace autofill {

class AutofillOptimizationGuideDecider;

// Singleton that owns all AutofillOptimizationGuideDeciders and associates them
// with Profiles. Listens for the Profile's destruction notification and cleans
// up the associated AutofillOptimizationGuideDecider.
class AutofillOptimizationGuideDeciderFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the AutofillOptimizationGuideDecider for `profile`, creating it if
  // it is not yet created.
  static AutofillOptimizationGuideDecider* GetForProfile(Profile* profile);

  // Gets the Singleton instance of the AutofillOptimizationGuideDeciderFactory.
  static AutofillOptimizationGuideDeciderFactory* GetInstance();

 private:
  friend base::NoDestructor<AutofillOptimizationGuideDeciderFactory>;

  AutofillOptimizationGuideDeciderFactory();
  ~AutofillOptimizationGuideDeciderFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_OPTIMIZATION_GUIDE_DECIDER_FACTORY_H_
