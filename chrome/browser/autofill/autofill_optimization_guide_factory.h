// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_OPTIMIZATION_GUIDE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_OPTIMIZATION_GUIDE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class KeyedService;
class Profile;

namespace autofill {

class AutofillOptimizationGuide;

// Singleton that owns all AutofillOptimizationGuides and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AutofillOptimizationGuide.
class AutofillOptimizationGuideFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the AutofillOptimizationGuide for `profile`, creating it if it is
  // not yet created.
  static AutofillOptimizationGuide* GetForProfile(Profile* profile);

  // Gets the Singleton instance of the AutofillOptimizationGuideFactory.
  static AutofillOptimizationGuideFactory* GetInstance();

 private:
  friend base::NoDestructor<AutofillOptimizationGuideFactory>;

  AutofillOptimizationGuideFactory();
  ~AutofillOptimizationGuideFactory() override;

  // ProfileKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_OPTIMIZATION_GUIDE_FACTORY_H_
