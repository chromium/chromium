// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_OFFER_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_OFFER_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace autofill {

class AutofillOfferManager;

// Singleton that owns all AutofillOfferManager and associates them with
// Profiles.
class AutofillOfferManagerFactory : public ProfileKeyedServiceFactory {
 public:
  AutofillOfferManagerFactory(const AutofillOfferManagerFactory&) = delete;
  AutofillOfferManagerFactory& operator=(const AutofillOfferManagerFactory&) =
      delete;

  static AutofillOfferManager* GetForBrowserContext(
      content::BrowserContext* context);

  static AutofillOfferManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<AutofillOfferManagerFactory>;

  AutofillOfferManagerFactory();
  ~AutofillOfferManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_OFFER_MANAGER_FACTORY_H_
