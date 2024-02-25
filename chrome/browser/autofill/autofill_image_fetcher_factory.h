// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace autofill {

class AutofillImageFetcherBase;

// Singleton that owns all AutofillImageFetchers and associates them with
// Profiles.
class AutofillImageFetcherFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the AutofillImageFetcher for |profile|, creating it if it is not
  // yet created.
  static AutofillImageFetcherBase* GetForProfile(Profile* profile);

  static AutofillImageFetcherFactory* GetInstance();

  static KeyedService* BuildAutofillImageFetcher(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<AutofillImageFetcherFactory>;

  AutofillImageFetcherFactory();
  ~AutofillImageFetcherFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
